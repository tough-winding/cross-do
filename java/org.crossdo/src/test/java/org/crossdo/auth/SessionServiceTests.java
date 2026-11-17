package org.crossdo.auth;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.infrastructure.DbopClient;
import org.springframework.http.HttpStatus;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.ValueOperations;
import org.springframework.web.server.ResponseStatusException;

import java.time.Duration;
import java.util.HashMap;
import java.util.Map;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;

class SessionServiceTests {
    private SessionService sessions;
    private final Map<String, String> values = new HashMap<>();
    private StringRedisTemplate redis;
    private DbopClient dbop;
    private final ObjectMapper mapper = new ObjectMapper();
    private static final String PATIENT = "11111111-1111-4111-8111-111111111111";
    private static final String VOLUNTEER = "22222222-2222-4222-8222-222222222222";

    @BeforeEach
    @SuppressWarnings("unchecked")
    void setup() {
        redis = mock(StringRedisTemplate.class);
        ValueOperations<String, String> operations = mock(ValueOperations.class);
        when(redis.opsForValue()).thenReturn(operations);
        doAnswer(invocation -> { values.put(invocation.getArgument(0), invocation.getArgument(1)); return null; })
                .when(operations).set(anyString(), anyString(), any(Duration.class));
        when(operations.get(anyString())).thenAnswer(invocation -> values.get(invocation.getArgument(0)));
        when(redis.delete(anyString())).thenAnswer(invocation -> values.remove(invocation.getArgument(0)) != null);
        dbop = mock(DbopClient.class);
        when(dbop.call(eq("security/version"), any(ObjectNode.class))).thenAnswer(invocation -> {
            ObjectNode body = invocation.getArgument(1);
            return mapper.createObjectNode().put("user_id", body.path("actor_id").asText()).put("session_version", 0);
        });
        when(dbop.call(eq("security/session"), any(ObjectNode.class))).thenAnswer(invocation -> {
            ObjectNode body = invocation.getArgument(1);
            return mapper.createObjectNode().put("user_id", body.path("actor_id").asText());
        });
        sessions = new SessionService(redis, Duration.ofHours(2), dbop, mapper);
    }

    @Test
    void createsOpaqueSessionsAndRevokesOnlyRequestedSession() {
        String first = sessions.create(PATIENT, "0");
        String second = sessions.create(VOLUNTEER, "0");
        assertNotEquals(first, second);
        assertEquals(43, first.length());
        assertFalse(values.keySet().stream().anyMatch(key -> key.contains(first)));
        assertEquals(PATIENT, sessions.currentUserId("Bearer " + first));
        sessions.logout("Bearer " + first);
        assertEquals(401, assertThrows(ResponseStatusException.class,
                () -> sessions.currentUserId("Bearer " + first)).getRawStatusCode());
        assertEquals(VOLUNTEER, sessions.currentUserId("Bearer " + second));
        assertEquals(7200, sessions.expiresIn());
    }

    @Test
    void rejectsMissingMalformedAndExpiredTokens() {
        for (String token : new String[]{null, "", "Bearer patient", "Basic abc", "Bearer " + "a".repeat(43)}) {
            assertEquals(401, assertThrows(ResponseStatusException.class,
                    () -> sessions.currentUserId(token)).getRawStatusCode());
        }
    }

    @Test
    void versionChangeRevokesEveryOlderToken() {
        String first = sessions.create(PATIENT, "0");
        String second = sessions.create(PATIENT, "0");
        when(dbop.call(eq("security/session"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.FORBIDDEN, "revoked"));
        for (String token : new String[]{first, second}) {
            assertEquals(401, assertThrows(ResponseStatusException.class,
                    () -> sessions.currentUserId("Bearer " + token)).getRawStatusCode());
        }
        assertTrue(values.isEmpty());
    }

    @Test
    void resetBetweenPasswordVerificationAndIssuanceCannotCreateANewSession() {
        when(dbop.call(eq("security/version"), any(ObjectNode.class)))
                .thenReturn(mapper.createObjectNode().put("user_id", PATIENT).put("session_version", 1));
        assertEquals(401, assertThrows(ResponseStatusException.class,
                () -> sessions.create(PATIENT, "0")).getRawStatusCode());
        assertTrue(values.isEmpty());
    }

    @Test
    void resetAfterIssuanceCheckStillLeavesTheTokenBoundToTheOldCredentials() {
        String token = sessions.create(PATIENT, "0");
        assertTrue(values.values().stream().allMatch(value -> value.endsWith(":0")));
        when(dbop.call(eq("security/session"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED, "version changed"));
        assertEquals(401, assertThrows(ResponseStatusException.class,
                () -> sessions.currentUserId("Bearer " + token)).getRawStatusCode());
        assertTrue(values.isEmpty());
    }

    @Test
    void aDatabaseOutagePreservesSessionsButNeverAuthorizesTheRequest() {
        String token = sessions.create(PATIENT, "0");
        when(dbop.call(eq("security/session"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "down"));
        assertEquals(503, assertThrows(ResponseStatusException.class,
                () -> sessions.currentUserId("Bearer " + token)).getRawStatusCode());
        assertEquals(1, values.size());
    }

    @Test
    void legacyUnversionedSessionsAreRejected() {
        String token = sessions.create(PATIENT, "0");
        values.replaceAll((key, value) -> PATIENT);
        assertEquals(401, assertThrows(ResponseStatusException.class,
                () -> sessions.currentUserId("Bearer " + token)).getRawStatusCode());
        assertTrue(values.isEmpty());
    }

    @Test
    void redisFailureDoesNotAllowLogin() {
        when(redis.opsForValue().get(anyString())).thenThrow(new org.springframework.data.redis.RedisConnectionFailureException("down"));
        assertEquals(503, assertThrows(ResponseStatusException.class,
                () -> sessions.currentUserId("Bearer " + "a".repeat(43))).getRawStatusCode());
    }
}
