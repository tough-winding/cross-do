package org.crossdo.infrastructure;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.DeserializationFeature;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ResponseStatusException;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;

/** Service-authenticated access to DBOP. A write is replayed only after a pre-operation 401. */
@Component
public class DbopClient {
    private static final Duration TOKEN_LIFETIME = Duration.ofMinutes(20);
    private static final DateTimeFormatter IAM_DATE = DateTimeFormatter
            .ofPattern("yyyy-MM-dd HH:mm:ss").withZone(ZoneOffset.UTC);

    private final ObjectMapper mapper;
    private final HttpClient http;
    private final String dbopBaseUrl;
    private final String iamBaseUrl;
    private final String serviceName;
    private final String servicePassword;
    private final Clock clock;
    private final Duration requestTimeout;
    private volatile Token cachedToken;

    @Autowired
    public DbopClient(ObjectMapper mapper,
                      @Value("${crossdo.dbop.base-url}") String dbopBaseUrl,
                      @Value("${crossdo.iam.base-url}") String iamBaseUrl,
                      @Value("${crossdo.service.name}") String serviceName,
                      @Value("${crossdo.service.password:}") String servicePassword) {
        this(mapper, HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(3)).build(),
                dbopBaseUrl, iamBaseUrl, serviceName, servicePassword, Clock.systemUTC(), Duration.ofSeconds(8));
    }

    // Injectable timing and transport keep protocol tests local and deterministic.
    DbopClient(ObjectMapper mapper, HttpClient http, String dbopBaseUrl, String iamBaseUrl,
               String serviceName, String servicePassword, Clock clock, Duration requestTimeout) {
        this.mapper = mapper;
        this.http = http;
        this.dbopBaseUrl = trimTrailingSlash(dbopBaseUrl);
        this.iamBaseUrl = trimTrailingSlash(iamBaseUrl);
        this.serviceName = serviceName;
        this.servicePassword = servicePassword;
        this.clock = clock;
        this.requestTimeout = requestTimeout;
    }

    public JsonNode call(String action, ObjectNode body) {
        if (servicePassword == null || servicePassword.isBlank() || serviceName == null || serviceName.isBlank()) {
            throw unavailable("未配置服务间认证凭据");
        }
        if (!serviceName.matches("[A-Za-z0-9_.-]{1,100}")) {
            throw unavailable("服务间认证名称配置不正确");
        }
        if (action == null || !action.matches("[a-z][a-z0-9_-]*(/[a-z][a-z0-9_-]*)*") || body == null) {
            throw new IllegalArgumentException("Invalid DBOP action or body");
        }
        String requestBody = json(body);
        String token = token(null);
        HttpResponse<String> response = executeDbop(action, requestBody, token);
        if (response.statusCode() == 401) {
            // DBOP authenticates before executing the operation, so this one replay is safe.
            token = token(token);
            response = executeDbop(action, requestBody, token);
            if (response.statusCode() == 401) {
                invalidate(token);
                throw unavailable("数据服务认证暂时不可用");
            }
        }
        int status = response.statusCode();
        if (status >= 400 && status < 500) {
            throw new ResponseStatusException(status, errorMessage(response.body()), null);
        }
        if (status < 200 || status >= 300) {
            throw unavailable("数据服务暂时不可用");
        }
        return responseJson(response.body(), "数据服务返回了无效响应");
    }

    private HttpResponse<String> executeDbop(String action, String body, String token) {
        return send(HttpRequest.newBuilder(endpoint(dbopBaseUrl, "/workflow/" + action))
                .timeout(requestTimeout)
                .header("Content-Type", "application/json")
                .header("Accept", "application/json")
                .header("ServiceName", serviceName)
                .header("Authorization", token)
                .POST(HttpRequest.BodyPublishers.ofString(body, StandardCharsets.UTF_8))
                .build());
    }

    /** rejectedToken is null for normal use; otherwise refresh once unless another thread already did. */
    private synchronized String token(String rejectedToken) {
        Token existing = cachedToken;
        if (existing != null && existing.expiresAt.isAfter(clock.instant())
                && (rejectedToken == null || !existing.value.equals(rejectedToken))) {
            return existing.value;
        }
        cachedToken = null;
        String timestamp = IAM_DATE.format(clock.instant());
        ObjectNode credentials = mapper.createObjectNode()
                .put("service_username", serviceName)
                .put("encrypt_str", sha256(serviceName + "+" + servicePassword + "+" + timestamp))
                .put("date", timestamp);
        HttpResponse<String> response = send(HttpRequest.newBuilder(endpoint(iamBaseUrl, "/get_token"))
                .timeout(requestTimeout)
                .header("Content-Type", "application/json")
                .header("Accept", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(json(credentials), StandardCharsets.UTF_8))
                .build());
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            cachedToken = null;
            throw unavailable("服务间认证暂时不可用");
        }
        JsonNode result = responseJson(response.body(), "认证服务返回了无效响应");
        JsonNode value = result.get("Token");
        if (value == null || !value.isTextual() || value.asText().isBlank()
                || !value.asText().matches("[A-Za-z0-9_-]{1,256}")) {
            cachedToken = null;
            throw unavailable("认证服务返回了无效响应");
        }
        String token = value.asText();
        cachedToken = new Token(token, clock.instant().plus(TOKEN_LIFETIME));
        return token;
    }

    private synchronized void invalidate(String token) {
        if (cachedToken != null && cachedToken.value.equals(token)) {
            cachedToken = null;
        }
    }

    private HttpResponse<String> send(HttpRequest request) {
        try {
            return http.send(request, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
            throw unavailable("数据服务请求被中断");
        } catch (IOException exception) {
            // A timeout can follow a committed write; do not automatically replay it.
            throw unavailable("数据服务暂时不可用");
        }
    }

    private String json(JsonNode body) {
        try {
            return mapper.writeValueAsString(body);
        } catch (JsonProcessingException exception) {
            throw unavailable("无法编码服务请求");
        }
    }

    private JsonNode responseJson(String body, String reason) {
        try {
            JsonNode result = mapper.reader().with(DeserializationFeature.FAIL_ON_TRAILING_TOKENS).readTree(body);
            if (result == null || (!result.isObject() && !result.isArray())) {
                throw unavailable(reason);
            }
            return result;
        } catch (IOException exception) {
            throw unavailable(reason);
        }
    }

    private String errorMessage(String body) {
        try {
            JsonNode result = mapper.readTree(body);
            JsonNode message = result == null ? null : result.get("message");
            if (message != null && message.isTextual() && !message.asText().isBlank()) {
                return message.asText();
            }
        } catch (IOException ignored) {
            // Legacy DBOP errors may have empty bodies; never echo unstructured internal content.
        }
        return "数据服务拒绝了请求";
    }

    private URI endpoint(String baseUrl, String path) {
        try {
            URI uri = URI.create(baseUrl + path);
            if (!("http".equals(uri.getScheme()) || "https".equals(uri.getScheme()))
                    || uri.getHost() == null || uri.getUserInfo() != null || uri.getQuery() != null
                    || uri.getFragment() != null) {
                throw unavailable("数据服务地址配置不正确");
            }
            return uri;
        } catch (IllegalArgumentException exception) {
            throw unavailable("数据服务地址配置不正确");
        }
    }

    private static String trimTrailingSlash(String value) {
        return value == null ? "" : value.replaceAll("/+$", "");
    }

    private static String sha256(String value) {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));
            StringBuilder result = new StringBuilder(64);
            for (byte octet : digest) {
                result.append(String.format("%02x", octet & 0xff));
            }
            return result.toString();
        } catch (NoSuchAlgorithmException exception) {
            throw new IllegalStateException("SHA-256 is unavailable");
        }
    }

    private static ResponseStatusException unavailable(String reason) {
        return new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, reason);
    }

    private static final class Token {
        private final String value;
        private final Instant expiresAt;

        private Token(String value, Instant expiresAt) {
            this.value = value;
            this.expiresAt = expiresAt;
        }
    }
}
