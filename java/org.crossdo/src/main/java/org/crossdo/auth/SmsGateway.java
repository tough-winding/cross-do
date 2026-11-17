package org.crossdo.auth;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.web.server.ResponseStatusException;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.Arrays;

/** An explicit development mode or a configured SMS delivery adapter. */
@Service
public class SmsGateway {
    private final String mode;
    private final String url;
    private final String key;
    private final ObjectMapper mapper;
    private final HttpClient client = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(5))
            .followRedirects(HttpClient.Redirect.NEVER).build();

    public SmsGateway(@Value("${CROSSDO_SMS_MODE:disabled}") String mode,
                      @Value("${CROSSDO_SMS_GATEWAY_URL:}") String url,
                      @Value("${CROSSDO_SMS_GATEWAY_KEY:}") String key, ObjectMapper mapper) {
        if (!Arrays.asList("disabled", "development", "gateway").contains(mode))
            throw new IllegalArgumentException("Unknown SMS mode");
        this.mode = mode;
        this.url = url;
        this.key = key;
        this.mapper = mapper;
    }

    public String mode() { return mode; }

    public void requireAvailable() {
        if ("disabled".equals(mode))
            throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "短信验证服务尚未配置");
        if ("gateway".equals(mode) && (url.isBlank() || key.isBlank()))
            throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "短信验证服务尚未配置");
    }

    public void send(String phone, String purpose, String code, String challengeId) {
        requireAvailable();
        if ("development".equals(mode)) return;
        try {
            URI endpoint = URI.create(url);
            if (!"https".equalsIgnoreCase(endpoint.getScheme()) || endpoint.getHost() == null || endpoint.getUserInfo() != null)
                throw new IllegalArgumentException("SMS gateway must use HTTPS without URL credentials");
            String body = mapper.createObjectNode().put("phone_number", phone).put("purpose", purpose)
                    .put("code", code).put("expires_in", 300).put("idempotency_key", challengeId).toString();
            HttpRequest request = HttpRequest.newBuilder(endpoint).timeout(Duration.ofSeconds(8))
                    .header("Authorization", "Bearer " + key).header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(body)).build();
            HttpResponse<Void> response = client.send(request, HttpResponse.BodyHandlers.discarding());
            if (response.statusCode() < 200 || response.statusCode() >= 300) throw new IllegalStateException("SMS delivery rejected");
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw unavailable();
        } catch (Exception e) {
            // Never log a verification code, API key, phone, or provider response.
            throw unavailable();
        }
    }

    private ResponseStatusException unavailable() {
        return new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "验证码暂时无法发送，请稍后重试");
    }
}
