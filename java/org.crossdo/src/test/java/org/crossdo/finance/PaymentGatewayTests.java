package org.crossdo.finance;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.springframework.http.HttpStatus;
import org.springframework.web.server.ResponseStatusException;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.io.IOException;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Clock;
import java.time.Instant;
import java.time.ZoneOffset;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;

class PaymentGatewayTests {
    private static final String ID="11111111-1111-4111-8111-111111111111";
    private static final String SECRET="test-webhook-secret-with-more-than-32-characters";
    private static final long NOW=1800000000L;
    private final ObjectMapper mapper=new ObjectMapper();
    private HttpClient http;

    @BeforeEach void setup(){http=mock(HttpClient.class);}
    private PaymentGateway gateway(String mode){return gateway(mode,"https://payments.example.test/api",SECRET);}
    private PaymentGateway gateway(String mode,String url,String secret){
        return new PaymentGateway(mapper,mode,url,"test-api-key",secret,http,Clock.fixed(Instant.ofEpochSecond(NOW),ZoneOffset.UTC));
    }
    private ObjectNode record(String mode){return mapper.createObjectNode().put("id",ID).put("project_id",ID)
            .put("mode",mode).put("amount","10.25").put("status","pending");}
    private ObjectNode callback(){return mapper.createObjectNode().put("event_id","event-001").put("type","payment")
            .put("reference_id",ID).put("status","succeeded").put("amount","10.25").put("currency","CNY");}
    private String sign(String raw,String timestamp)throws Exception{
        Mac mac=Mac.getInstance("HmacSHA256");mac.init(new SecretKeySpec(SECRET.getBytes(StandardCharsets.UTF_8),"HmacSHA256"));
        return PaymentGateway.hex(mac.doFinal((timestamp+"."+raw).getBytes(StandardCharsets.UTF_8)));
    }
    private ObjectNode verifyRaw(String raw)throws Exception{return gateway("gateway").verifiedCallback(raw,Long.toString(NOW),sign(raw,Long.toString(NOW)));}
    private void assertStatus(HttpStatus status,Runnable action){assertEquals(status,assertThrows(ResponseStatusException.class,action::run).getStatus());}

    @Test void disabledAndIncompleteConfigurationFailClosed(){
        assertStatus(HttpStatus.SERVICE_UNAVAILABLE,()->gateway("disabled").requireEnabled());
        assertStatus(HttpStatus.SERVICE_UNAVAILABLE,()->gateway("gateway","http://payments.example.test",SECRET).requireEnabled());
        assertStatus(HttpStatus.SERVICE_UNAVAILABLE,()->gateway("gateway","https://payments.example.test","short").requireEnabled());
        assertThrows(IllegalArgumentException.class,()->gateway("production"));
        verifyNoInteractions(http);
    }
    @Test void developmentNeverCallsExternalGatewayAndEventsRemainExplicitlySimulated(){
        PaymentGateway development=gateway("development");
        assertTrue(development.dispatch("payment",record("development")).isObject());
        ObjectNode first=development.developmentEvent("payment",record("development"),"donor");
        ObjectNode retry=development.developmentEvent("payment",record("development"),"administrator");
        assertEquals("development",first.path("mode").asText());
        assertTrue(first.path("event_id").asText().startsWith("development:"));
        assertEquals(first.path("payload_hash"),retry.path("payload_hash"));
        assertStatus(HttpStatus.FORBIDDEN,()->gateway("gateway").developmentEvent("payment",record("development"),"donor"));
        assertStatus(HttpStatus.FORBIDDEN,()->development.developmentEvent("payment",record("gateway"),"donor"));
        assertStatus(HttpStatus.CONFLICT,()->development.dispatch("payment",record("gateway")));
        verifyNoInteractions(http);
    }
    @Test void validSignedRawCallbackIsWhitelistedAndCannotInjectActorOrMode()throws Exception{
        String raw=callback().put("actor_id","attacker").put("mode","development").put("amount_override","1").toString();
        ObjectNode verified=verifyRaw(raw);
        assertEquals("10.25",verified.path("amount").asText());
        assertEquals("gateway",verified.path("mode").asText());
        assertEquals(PaymentGateway.sha256(raw),verified.path("payload_hash").asText());
        assertFalse(verified.has("actor_id"));assertFalse(verified.has("amount_override"));
    }
    @Test void changedRawBodyAndExpiredOrFutureSignaturesAreRejected()throws Exception{
        String raw=callback().toString(),stamp=Long.toString(NOW);
        String signature=sign(raw,stamp);
        assertStatus(HttpStatus.UNAUTHORIZED,()->gateway("gateway").verifiedCallback(raw+" ",stamp,signature));
        for(long instant:new long[]{NOW-301,NOW+301}){
            String time=Long.toString(instant),signed=sign(raw,time);
            assertStatus(HttpStatus.UNAUTHORIZED,()->gateway("gateway").verifiedCallback(raw,time,signed));
        }
        assertStatus(HttpStatus.UNAUTHORIZED,()->gateway("gateway").verifiedCallback(raw,"invalid",signature));
        assertStatus(HttpStatus.UNAUTHORIZED,()->gateway("gateway").verifiedCallback(raw,stamp,null));
        assertStatus(HttpStatus.FORBIDDEN,()->gateway("development").verifiedCallback(raw,stamp,signature));
    }
    @Test void callbackRejectsZeroOverprecisionNondecimalAndWrongCurrency()throws Exception{
        for(String amount:new String[]{"0","0.00","1.001","-1","1e2","100000000","01.00"}){
            String raw=callback().put("amount",amount).toString(),stamp=Long.toString(NOW),signature=sign(raw,stamp);
            assertStatus(HttpStatus.BAD_REQUEST,()->gateway("gateway").verifiedCallback(raw,stamp,signature));
        }
        String raw=callback().put("currency","USD").toString(),stamp=Long.toString(NOW),signature=sign(raw,stamp);
        assertStatus(HttpStatus.BAD_REQUEST,()->gateway("gateway").verifiedCallback(raw,stamp,signature));
    }
    @Test void callbackRequiresCanonicalIdentifiersTextFieldsAndSingleJsonDocument()throws Exception{
        String[] malformed={callback().put("reference_id","not-a-uuid").toString(),
                callback().put("amount",10.25).toString(),callback().put("status","paid").toString(),
                callback().put("event_id","bad event").toString(),callback().toString()+" {}"};
        for(String raw:malformed){String stamp=Long.toString(NOW),signature=sign(raw,stamp);
            assertStatus(HttpStatus.BAD_REQUEST,()->gateway("gateway").verifiedCallback(raw,stamp,signature));}
    }
    @Test void validGatewayRequestUsesStableReferenceAndIdempotencyHeader()throws Exception{
        @SuppressWarnings("unchecked") HttpResponse<String> response=mock(HttpResponse.class);
        when(response.statusCode()).thenReturn(200);when(response.body()).thenReturn("{\"checkout_url\":\"https://checkout.example.test/pay\"}");
        when(http.send(any(HttpRequest.class),org.mockito.ArgumentMatchers.<HttpResponse.BodyHandler<String>>any())).thenReturn(response);
        JsonNode checkout=gateway("gateway").dispatch("payment",record("gateway"));
        assertEquals("https://checkout.example.test/pay",checkout.path("checkout_url").asText());
        ArgumentCaptor<HttpRequest> request=ArgumentCaptor.forClass(HttpRequest.class);
        verify(http).send(request.capture(),org.mockito.ArgumentMatchers.<HttpResponse.BodyHandler<String>>any());
        assertEquals("https://payments.example.test/api/payments",request.getValue().uri().toString());
        assertEquals(ID,request.getValue().headers().firstValue("Idempotency-Key").orElseThrow());
        assertEquals("POST",request.getValue().method());
    }
    @Test void gatewayTimeoutIsUncertainAndDoesNotPretendSuccess()throws Exception{
        when(http.send(any(HttpRequest.class),org.mockito.ArgumentMatchers.<HttpResponse.BodyHandler<String>>any())).thenThrow(new IOException("timeout"));
        ResponseStatusException failure=assertThrows(ResponseStatusException.class,()->gateway("gateway").dispatch("refund",record("gateway")));
        assertEquals(HttpStatus.SERVICE_UNAVAILABLE,failure.getStatus());
        assertTrue(failure.getReason().contains("待确认"));
    }
    @Test void checkoutAndGatewayUrlsRequireHttpsWithoutCredentials(){
        for(String url:new String[]{"http://checkout.example.test","javascript:alert(1)","https://secret@example.test/pay","/relative"})
            assertStatus(HttpStatus.SERVICE_UNAVAILABLE,()->PaymentGateway.validateCheckout(url));
        for(String url:new String[]{"https://payments.example.test?token=x","https://secret@payments.example.test","https://payments.example.test#fragment"})
            assertStatus(HttpStatus.SERVICE_UNAVAILABLE,()->gateway("gateway",url,SECRET).requireEnabled());
    }
}
