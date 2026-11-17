package org.crossdo.finance;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.DeserializationFeature;
import java.math.BigDecimal;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.web.server.ResponseStatusException;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;

/** Configurable gateway contract, deliberately separate from a specific payment vendor. */
@Service
public class PaymentGateway {
    private final ObjectMapper mapper;
    private final String mode, baseUrl, apiKey, webhookSecret;
    private final HttpClient http;
    private final Clock clock;

    @Autowired
    public PaymentGateway(ObjectMapper mapper,
            @Value("${crossdo.payment.mode:${CROSSDO_PAYMENT_MODE:disabled}}") String mode,
            @Value("${crossdo.payment.gateway-url:${CROSSDO_PAYMENT_GATEWAY_URL:}}") String baseUrl,
            @Value("${crossdo.payment.gateway-key:${CROSSDO_PAYMENT_GATEWAY_KEY:}}") String apiKey,
            @Value("${crossdo.payment.webhook-secret:${CROSSDO_PAYMENT_WEBHOOK_SECRET:}}") String webhookSecret) {
        this(mapper, mode, baseUrl, apiKey, webhookSecret,
                HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(3)).followRedirects(HttpClient.Redirect.NEVER).build(), Clock.systemUTC());
    }

    PaymentGateway(ObjectMapper mapper, String mode, String baseUrl, String apiKey, String webhookSecret, HttpClient http, Clock clock) {
        if (!("disabled".equals(mode) || "development".equals(mode) || "gateway".equals(mode)))
            throw new IllegalArgumentException("Unknown payment mode");
        this.mapper=mapper; this.mode=mode; this.baseUrl=baseUrl.replaceAll("/+$","");
        this.apiKey=apiKey; this.webhookSecret=webhookSecret; this.http=http; this.clock=clock;
    }

    public String mode() { return mode; }

    public void requireEnabled() {
        if ("disabled".equals(mode)) throw unavailable("支付功能尚未配置");
        if ("gateway".equals(mode)) {
            gatewayUri("/payments");
            if (apiKey.isBlank() || webhookSecret.length()<32)
                throw unavailable("支付网关及回调密钥尚未完整配置");
        }
    }

    public void requireDevelopment(String storedMode) {
        if (!"development".equals(mode) || !"development".equals(storedMode))
            throw new ResponseStatusException(HttpStatus.FORBIDDEN,"模拟支付仅在开发模式下可用");
    }

    public void requireMatchingMode(JsonNode record) {
        requireEnabled();
        if (!mode.equals(record.path("mode").asText()))
            throw new ResponseStatusException(HttpStatus.CONFLICT,"当前服务与这笔记录的支付模式不一致");
    }

    /** A timeout leaves the durable request pending. The same reference is reused on retries. */
    public JsonNode dispatch(String type, JsonNode record) {
        requireMatchingMode(record);
        if ("development".equals(mode)) return mapper.createObjectNode();
        String endpoint = "payment".equals(type)?"/payments":"refund".equals(type)?"/refunds":"/disbursements";
        ObjectNode body=mapper.createObjectNode().put("reference_id",record.path("id").asText())
                .put("idempotency_key",record.path("id").asText()).put("amount",record.path("amount").asText())
                .put("currency","CNY").put("project_id",record.path("project_id").asText());
        if ("refund".equals(type)) body.put("payment_reference_id",record.path("order_id").asText());
        if ("disbursement".equals(type)) body.put("payout_reference",record.path("payout_reference").asText());
        try {
            HttpRequest request=HttpRequest.newBuilder(gatewayUri(endpoint)).timeout(Duration.ofSeconds(8))
                    .header("Authorization","Bearer "+apiKey).header("Content-Type","application/json")
                    .header("Idempotency-Key",record.path("id").asText())
                    .POST(HttpRequest.BodyPublishers.ofString(mapper.writeValueAsString(body),StandardCharsets.UTF_8)).build();
            HttpResponse<String> response=http.send(request,HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
            if (response.statusCode()<200 || response.statusCode()>=300 || response.body().length()>1024*1024)
                throw unavailable("支付服务尚未确认，请保留当前订单并重试");
            JsonNode result=mapper.readTree(response.body());
            if (result==null || !result.isObject()) throw unavailable("支付服务返回了无效结果");
            if (result.hasNonNull("checkout_url")) validateCheckout(result.get("checkout_url").asText());
            return result;
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();throw unavailable("支付服务请求被中断，订单仍待确认");
        } catch (IOException | IllegalArgumentException e) {
            throw unavailable("支付服务暂不可用，订单仍待确认");
        }
    }

    public ObjectNode verifiedCallback(String raw, String timestamp, String signature) {
        if (!"gateway".equals(mode) || webhookSecret.length()<32)
            throw new ResponseStatusException(HttpStatus.FORBIDDEN,"支付回调未启用");
        if (raw==null || raw.getBytes(StandardCharsets.UTF_8).length>65536 || timestamp==null || signature==null)
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED,"支付回调签名无效");
        long signedAt;
        try { signedAt=Long.parseLong(timestamp); } catch(NumberFormatException e) { throw unauthorized(); }
        long now=Instant.now(clock).getEpochSecond();
        if (signedAt<now-300 || signedAt>now+300 || !signature.matches("[0-9a-fA-F]{64}")) throw unauthorized();
        try {
            Mac mac=Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(webhookSecret.getBytes(StandardCharsets.UTF_8),"HmacSHA256"));
            byte[] expected=mac.doFinal((timestamp+"."+raw).getBytes(StandardCharsets.UTF_8));
            if (!MessageDigest.isEqual(expected,unhex(signature))) throw unauthorized();
            JsonNode parsed=mapper.reader().with(DeserializationFeature.FAIL_ON_TRAILING_TOKENS).readTree(raw);
            if (parsed==null || !parsed.isObject()) throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"回调内容格式错误");
            ObjectNode result=mapper.createObjectNode();
            for (String key:new String[]{"event_id","type","reference_id","status","amount","currency"}) {
                if (!parsed.path(key).isTextual()) throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"回调字段必须为字符串");
                result.put(key,parsed.get(key).asText());
            }
            if (!result.path("event_id").asText().matches("[A-Za-z0-9._:-]{1,128}")
                    || !result.path("reference_id").asText().matches(FinanceController.UUID_PATTERN)
                    || !result.path("type").asText().matches("payment|refund|disbursement")
                    || !result.path("status").asText().matches("succeeded|failed")
                    || !result.path("amount").asText().matches("(?:0|[1-9][0-9]{0,7})(?:\\.[0-9]{1,2})?")
                    || new BigDecimal(result.path("amount").asText()).signum() <= 0
                    || !"CNY".equals(result.path("currency").asText()))
                throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"回调字段不正确");
            result.put("mode","gateway").put("payload_hash",sha256(raw));
            return result;
        } catch (ResponseStatusException e) { throw e; }
        catch (Exception e) { throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"无法验证支付回调"); }
    }

    public ObjectNode developmentEvent(String type, JsonNode record, String actor) {
        requireDevelopment(record.path("mode").asText());
        ObjectNode event=mapper.createObjectNode().put("event_id","development:"+type+":"+record.path("id").asText())
                .put("type",type).put("reference_id",record.path("id").asText()).put("status","succeeded")
                .put("amount",record.path("amount").asText()).put("currency","CNY").put("mode","development");
        event.put("payload_hash",sha256(event.toString())).put("actor_id",actor);
        return event;
    }

    private URI gatewayUri(String path) {
        URI uri;
        try { uri=URI.create(baseUrl+path); } catch(IllegalArgumentException e) { throw unavailable("支付网关地址不正确"); }
        if (!"https".equals(uri.getScheme()) || uri.getHost()==null || uri.getUserInfo()!=null || uri.getQuery()!=null || uri.getFragment()!=null)
            throw unavailable("支付网关必须使用完整 HTTPS 地址");
        return uri;
    }
    static void validateCheckout(String url) {
        URI uri;
        try {uri=URI.create(url);} catch(IllegalArgumentException e){throw unavailable("支付跳转地址不正确");}
        if (!"https".equals(uri.getScheme()) || uri.getHost()==null || uri.getUserInfo()!=null || url.length()>2048)
            throw unavailable("支付跳转地址不正确");
    }
    static String sha256(String value) {
        try {byte[] hash=MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));return hex(hash);}
        catch(Exception e){throw new IllegalStateException(e);}
    }
    static String hex(byte[] bytes) {StringBuilder b=new StringBuilder();for(byte v:bytes)b.append(String.format("%02x",v&255));return b.toString();}
    private static byte[] unhex(String hex){byte[] out=new byte[hex.length()/2];for(int i=0;i<out.length;i++)out[i]=(byte)Integer.parseInt(hex.substring(i*2,i*2+2),16);return out;}
    private static ResponseStatusException unauthorized(){return new ResponseStatusException(HttpStatus.UNAUTHORIZED,"支付回调签名无效或已过期");}
    private static ResponseStatusException unavailable(String reason){return new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE,reason);}
}
