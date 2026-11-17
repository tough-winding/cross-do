package org.crossdo.infrastructure;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.web.server.ResponseStatusException;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.http.HttpClient;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.ZoneId;
import java.time.ZoneOffset;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.jupiter.api.Assertions.*;

class DbopClientTest {
    private final ObjectMapper mapper = new ObjectMapper();
    private final AtomicInteger iamCalls = new AtomicInteger();
    private final AtomicInteger dbopCalls = new AtomicInteger();
    private final List<Request> iamRequests = Collections.synchronizedList(new ArrayList<>());
    private final List<Request> dbopRequests = Collections.synchronizedList(new ArrayList<>());
    private final MutableClock clock = new MutableClock(Instant.parse("2026-11-06T08:01:02Z"));
    private HttpServer server;
    private ExecutorService executor;
    private String baseUrl;
    private Handler iam;
    private Handler dbop;

    @BeforeEach
    void startServer() throws IOException {
        server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
        executor = Executors.newCachedThreadPool();
        server.setExecutor(executor);
        iam = (exchange, count) -> reply(exchange, 200, "{\"Token\":\"token-" + count + "\"}");
        dbop = (exchange, count) -> reply(exchange, 200, "{\"ok\":true}");
        server.createContext("/get_token", exchange -> {
            int count = iamCalls.incrementAndGet();
            iamRequests.add(record(exchange));
            iam.handle(exchange, count);
        });
        server.createContext("/workflow/", exchange -> {
            int count = dbopCalls.incrementAndGet();
            dbopRequests.add(record(exchange));
            dbop.handle(exchange, count);
        });
        server.start();
        baseUrl = "http://127.0.0.1:" + server.getAddress().getPort();
    }

    @AfterEach
    void stopServer() {
        server.stop(0);
        executor.shutdownNow();
    }

    @Test
    void speaksIamProtocolAndCachesServiceToken() throws Exception {
        DbopClient client = client(Duration.ofSeconds(2));
        JsonNode result = client.call("project/create", mapper.createObjectNode().put("actor_id", "患者"));
        client.call("project/list", mapper.createObjectNode());

        assertTrue(result.get("ok").asBoolean());
        assertEquals(1, iamCalls.get());
        assertEquals(2, dbopCalls.get());
        Request request = iamRequests.get(0);
        assertEquals("POST", request.method);
        assertEquals("/get_token", request.path);
        assertEquals("application/json", request.contentType);
        assertEquals("CROSSDO_WEB", request.body.get("service_username").asText());
        assertEquals("2026-11-06 08:01:02", request.body.get("date").asText());
        byte[] expected = MessageDigest.getInstance("SHA-256")
                .digest("CROSSDO_WEB+测试密码+2026-11-06 08:01:02".getBytes(StandardCharsets.UTF_8));
        StringBuilder hex = new StringBuilder();
        for (byte octet : expected) hex.append(String.format("%02x", octet & 0xff));
        assertEquals(hex.toString(), request.body.get("encrypt_str").asText());
        assertFalse(request.body.has("password"));
        Request operation = dbopRequests.get(0);
        assertEquals("POST", operation.method);
        assertEquals("/workflow/project/create", operation.path);
        assertEquals("CROSSDO_WEB", operation.serviceName);
        assertEquals("token-1", operation.authorization);
        assertEquals("患者", operation.body.get("actor_id").asText());
    }

    @Test
    void acceptsReleaseActionNamesWithoutAllowingPathEscapes() {
        DbopClient client = client(Duration.ofSeconds(2));
        for (String action : new String[]{"security/reset-password", "security/change-phone",
                "ops/account-state", "ops/notifications-unread", "ops/case-create"}) {
            assertTrue(client.call(action, mapper.createObjectNode()).path("ok").asBoolean());
        }
        assertEquals("/workflow/ops/case-create", dbopRequests.get(4).path);
        for (String action : new String[]{"../credentials", "/ops/users", "ops//users",
                "ops/users?admin=true", "ops/%2e%2e/users", "ops/users#fragment"}) {
            assertThrows(IllegalArgumentException.class, () -> client.call(action, mapper.createObjectNode()));
        }
        assertEquals(5, dbopCalls.get());
    }

    @Test
    void renewsTokenAfterTwentyMinutes() {
        DbopClient client = client(Duration.ofSeconds(2));
        client.call("account/get", mapper.createObjectNode());
        clock.advance(Duration.ofMinutes(19));
        client.call("account/get", mapper.createObjectNode());
        assertEquals(1, iamCalls.get());
        clock.advance(Duration.ofMinutes(1));
        client.call("account/get", mapper.createObjectNode());
        assertEquals(2, iamCalls.get());
        assertEquals("token-2", dbopRequests.get(2).authorization);
    }

    @Test
    void refreshesAndReplaysExactlyOnceForPreOperationUnauthorized() {
        dbop = (exchange, count) -> reply(exchange, count == 1 ? 401 : 200,
                count == 1 ? "" : "{\"created\":true}");
        DbopClient client = client(Duration.ofSeconds(2));
        JsonNode result = client.call("project/create", mapper.createObjectNode().put("actor_id", "actor"));
        assertTrue(result.get("created").asBoolean());
        assertEquals(2, iamCalls.get());
        assertEquals(2, dbopCalls.get());
        assertEquals("token-1", dbopRequests.get(0).authorization);
        assertEquals("token-2", dbopRequests.get(1).authorization);
        assertEquals(dbopRequests.get(0).body, dbopRequests.get(1).body);
    }

    @Test
    void internalUnauthorizedBecomesUnavailableAfterOneRefresh() {
        dbop = (exchange, count) -> reply(exchange, 401, "{\"message\":\"internal credential rejected\"}");
        ResponseStatusException failure = assertThrows(ResponseStatusException.class,
                () -> client(Duration.ofSeconds(2)).call("project/create", mapper.createObjectNode()));
        assertEquals(503, failure.getRawStatusCode());
        assertEquals(2, iamCalls.get());
        assertEquals(2, dbopCalls.get());
        assertFalse(failure.getReason().contains("credential rejected"));
    }

    @Test
    void preservesValidationAndConflictWithoutReplayingWrites() {
        dbop = (exchange, count) -> reply(exchange, count == 1 ? 400 : 409,
                count == 1 ? "{\"message\":\"资料不完整\"}" : "{\"message\":\"任务已领取\"}");
        DbopClient client = client(Duration.ofSeconds(2));
        ResponseStatusException invalid = assertThrows(ResponseStatusException.class,
                () -> client.call("project/submit", mapper.createObjectNode()));
        assertEquals(400, invalid.getRawStatusCode());
        assertEquals("资料不完整", invalid.getReason());
        ResponseStatusException conflict = assertThrows(ResponseStatusException.class,
                () -> client.call("project/claim", mapper.createObjectNode()));
        assertEquals(409, conflict.getRawStatusCode());
        assertEquals("任务已领取", conflict.getReason());
        assertEquals(2, dbopCalls.get());
        assertEquals(1, iamCalls.get());
    }

    @Test
    void mapsServerFailureWithoutRetryOrExposingInternalResponse() {
        dbop = (exchange, count) -> reply(exchange, 500, "{\"message\":\"private SQL details\"}");
        ResponseStatusException failure = assertThrows(ResponseStatusException.class,
                () -> client(Duration.ofSeconds(2)).call("project/create", mapper.createObjectNode()));
        assertEquals(503, failure.getRawStatusCode());
        assertEquals(1, dbopCalls.get());
        assertFalse(failure.getReason().contains("private"));
    }

    @Test
    void doesNotReplayTimedOutWrite() {
        dbop = (exchange, count) -> {
            try {
                Thread.sleep(900);
                reply(exchange, 200, "{\"ok\":true}");
            } catch (InterruptedException exception) {
                Thread.currentThread().interrupt();
            } catch (IOException ignored) {
                // The request timeout closed the connection after the server received this write.
            }
        };
        ResponseStatusException failure = assertThrows(ResponseStatusException.class,
                () -> client(Duration.ofMillis(300)).call("project/create", mapper.createObjectNode()));
        assertEquals(503, failure.getRawStatusCode());
        assertEquals(1, dbopCalls.get());
        assertEquals(1, iamCalls.get());
    }

    @Test
    void doesNotReplayWriteAfterConnectionClosesWithoutResponse() {
        dbop = (exchange, count) -> exchange.close();
        ResponseStatusException failure = assertThrows(ResponseStatusException.class,
                () -> client(Duration.ofSeconds(2)).call("project/create", mapper.createObjectNode()));
        assertEquals(503, failure.getRawStatusCode());
        assertEquals(1, dbopCalls.get());
        assertEquals(1, iamCalls.get());
    }

    @Test
    void rejectsEmptyOrMalformedSuccessResponses() {
        String[] bodies = {"", "not-json", "null", "true", "{} garbage"};
        dbop = (exchange, count) -> reply(exchange, 200, bodies[count - 1]);
        DbopClient client = client(Duration.ofSeconds(2));
        for (String ignored : bodies) {
            ResponseStatusException failure = assertThrows(ResponseStatusException.class,
                    () -> client.call("project/create", mapper.createObjectNode()));
            assertEquals(503, failure.getRawStatusCode());
        }
        assertEquals(5, dbopCalls.get());
        assertEquals(1, iamCalls.get());
    }

    @Test
    void keepsUnconfiguredCredentialsLazyAndAvoidsAnyNetworkCall() {
        DbopClient client = new DbopClient(mapper, HttpClient.newHttpClient(), baseUrl, baseUrl,
                "CROSSDO_WEB", "", clock, Duration.ofSeconds(2));
        ResponseStatusException failure = assertThrows(ResponseStatusException.class,
                () -> client.call("account/get", mapper.createObjectNode()));
        assertEquals(503, failure.getRawStatusCode());
        assertTrue(failure.getReason().contains("未配置"));
        assertEquals(0, iamCalls.get());
        assertEquals(0, dbopCalls.get());
    }

    @Test
    void refusesIamFailuresWithoutExposingCredentialsOrCallingDbop() {
        iam = (exchange, count) -> reply(exchange, 400, "{\"password\":\"sensitive value\"}");
        ResponseStatusException failure = assertThrows(ResponseStatusException.class,
                () -> client(Duration.ofSeconds(2)).call("account/get", mapper.createObjectNode()));
        assertEquals(503, failure.getRawStatusCode());
        assertFalse(failure.getReason().contains("sensitive"));
        assertEquals(1, iamCalls.get());
        assertEquals(0, dbopCalls.get());
    }

    @Test
    void malformedIamResponseNeverReachesDbop() {
        iam = (exchange, count) -> reply(exchange, 200, "{\"Token\":\"\"}");
        ResponseStatusException failure = assertThrows(ResponseStatusException.class,
                () -> client(Duration.ofSeconds(2)).call("account/get", mapper.createObjectNode()));
        assertEquals(503, failure.getRawStatusCode());
        assertEquals(1, iamCalls.get());
        assertEquals(0, dbopCalls.get());
    }

    private DbopClient client(Duration requestTimeout) {
        return new DbopClient(mapper, HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(3)).build(),
                baseUrl + "/", baseUrl + "/", "CROSSDO_WEB", "测试密码", clock, requestTimeout);
    }

    private Request record(HttpExchange exchange) throws IOException {
        return new Request(exchange.getRequestMethod(), exchange.getRequestURI().toString(),
                exchange.getRequestHeaders().getFirst("Content-Type"),
                exchange.getRequestHeaders().getFirst("ServiceName"),
                exchange.getRequestHeaders().getFirst("Authorization"),
                mapper.readTree(exchange.getRequestBody()));
    }

    private static void reply(HttpExchange exchange, int status, String body) throws IOException {
        byte[] bytes = body.getBytes(StandardCharsets.UTF_8);
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.sendResponseHeaders(status, bytes.length == 0 ? -1 : bytes.length);
        if (bytes.length > 0) exchange.getResponseBody().write(bytes);
        exchange.close();
    }

    @FunctionalInterface
    private interface Handler {
        void handle(HttpExchange exchange, int count) throws IOException;
    }

    private static final class Request {
        final String method;
        final String path;
        final String contentType;
        final String serviceName;
        final String authorization;
        final JsonNode body;

        Request(String method, String path, String contentType, String serviceName,
                String authorization, JsonNode body) {
            this.method = method;
            this.path = path;
            this.contentType = contentType;
            this.serviceName = serviceName;
            this.authorization = authorization;
            this.body = body;
        }
    }

    private static final class MutableClock extends Clock {
        private Instant now;

        MutableClock(Instant now) {
            this.now = now;
        }

        void advance(Duration duration) {
            now = now.plus(duration);
        }

        @Override
        public ZoneId getZone() {
            return ZoneOffset.UTC;
        }

        @Override
        public Clock withZone(ZoneId zone) {
            return this;
        }

        @Override
        public Instant instant() {
            return now;
        }
    }
}
