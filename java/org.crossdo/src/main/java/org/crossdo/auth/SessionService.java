package org.crossdo.auth;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.crossdo.infrastructure.DbopClient;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.dao.DataAccessException;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.script.DefaultRedisScript;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.web.server.ResponseStatusException;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.time.Duration;
import java.util.Base64;
import java.util.Collections;

/** Opaque, expiring sessions shared across API instances through Redis. */
@Service
public class SessionService {
    private static final SecureRandom RANDOM = new SecureRandom();
    private static final DefaultRedisScript<Long> LOGIN_FAILURE = new DefaultRedisScript<>(
            "local n=redis.call('INCR',KEYS[1]); " +
            "if n==1 then redis.call('EXPIRE',KEYS[1],ARGV[1]); end; return n", Long.class);
    private final StringRedisTemplate redis;
    private final Duration ttl;
    private final DbopClient dbop;
    private final ObjectMapper mapper;

    public SessionService(StringRedisTemplate redis, @Value("${crossdo.session.ttl:PT2H}") Duration ttl,
                          DbopClient dbop, ObjectMapper mapper) {
        this.redis = redis;
        if (ttl.isNegative() || ttl.isZero()) throw new IllegalArgumentException("Session TTL must be positive");
        this.ttl = ttl;
        this.dbop = dbop;
        this.mapper = mapper;
    }

    public String create(String userId, String authenticatedVersion) {
        if (authenticatedVersion == null || !authenticatedVersion.matches("0|[1-9][0-9]{0,19}"))
            throw unavailable();
        JsonNode state = dbop.call("security/version", mapper.createObjectNode().put("actor_id", userId));
        String version = state.path("session_version").asText("");
        if (!userId.equals(state.path("user_id").asText()) || !version.matches("0|[1-9][0-9]{0,19}"))
            throw unavailable();
        // Bind to the credentials that were actually verified. A reset between
        // password verification and token issuance must not renew old authority.
        if (!authenticatedVersion.equals(version)) throw expired();
        byte[] bytes = new byte[32];
        RANDOM.nextBytes(bytes);
        String token = Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
        try { redis.opsForValue().set(sessionKey(token), "v1:" + userId + ":" + authenticatedVersion, ttl); }
        catch (DataAccessException e) { throw unavailable(); }
        return token;
    }

    public long expiresIn() { return ttl.getSeconds(); }

    public String currentUserId(String authorization) {
        String token = bearer(authorization);
        String stored;
        try { stored = redis.opsForValue().get(sessionKey(token)); }
        catch (DataAccessException e) { throw unavailable(); }
        if (stored == null) throw expired();
        String[] parts = stored.split(":", -1);
        if (parts.length != 3 || !parts[0].equals("v1") ||
                !parts[1].matches("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}") ||
                !parts[2].matches("0|[1-9][0-9]{0,19}")) {
            deleteSession(token);
            throw expired();
        }
        try {
            JsonNode result = dbop.call("security/session", mapper.createObjectNode()
                    .put("actor_id", parts[1]).put("session_version", parts[2]));
            if (!parts[1].equals(result.path("user_id").asText())) throw unavailable();
        } catch (ResponseStatusException e) {
            if (e.getStatus() == HttpStatus.UNAUTHORIZED || e.getStatus() == HttpStatus.FORBIDDEN ||
                    e.getStatus() == HttpStatus.NOT_FOUND) {
                deleteSession(token);
                throw expired();
            }
            // Temporary outages must not destroy a still valid session.
            throw e;
        }
        return parts[1];
    }

    public void logout(String authorization) {
        String token = bearer(authorization);
        deleteSession(token);
    }

    private void deleteSession(String token) {
        try { redis.delete(sessionKey(token)); }
        catch (DataAccessException e) { throw unavailable(); }
    }

    private ResponseStatusException expired() {
        return new ResponseStatusException(HttpStatus.UNAUTHORIZED, "登录已过期或已撤销，请重新登录");
    }

    public void checkLoginLimit(String phone) {
        try {
            String failures = redis.opsForValue().get(loginKey(phone));
            if (failures != null && Long.parseLong(failures) >= 10)
                throw new ResponseStatusException(HttpStatus.TOO_MANY_REQUESTS, "登录失败次数过多，请稍后再试");
        } catch (DataAccessException e) { throw unavailable(); }
    }

    public void loginFailed(String phone) {
        try {
            redis.execute(LOGIN_FAILURE, Collections.singletonList(loginKey(phone)), "900");
        } catch (DataAccessException e) { throw unavailable(); }
    }

    public void loginSucceeded(String phone) {
        try { redis.delete(loginKey(phone)); }
        catch (DataAccessException e) { throw unavailable(); }
    }

    private String bearer(String authorization) {
        if (authorization == null || !authorization.matches("Bearer [A-Za-z0-9_-]{43}"))
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "请先登录");
        return authorization.substring(7);
    }

    private String sessionKey(String token) { return "crossdo:session:" + digest(token); }
    private String loginKey(String phone) { return "crossdo:login-failures:" + digest(phone); }

    private String digest(String value) {
        try {
            byte[] bytes = MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));
            return Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
        } catch (NoSuchAlgorithmException e) { throw new IllegalStateException(e); }
    }

    private ResponseStatusException unavailable() {
        return new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "登录服务暂不可用");
    }
}
