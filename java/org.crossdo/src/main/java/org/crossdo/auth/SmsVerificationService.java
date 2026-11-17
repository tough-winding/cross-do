package org.crossdo.auth;

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
import java.util.Arrays;
import java.util.Base64;
import java.util.Collections;
import java.util.UUID;

/** Codes are hashed, bounded, purpose-bound, and consumed once by one Redis script. */
@Service
public class SmsVerificationService {
    private static final SecureRandom RANDOM = new SecureRandom();
    private static final DefaultRedisScript<Long> ISSUE = new DefaultRedisScript<>(
            "if redis.call('EXISTS',KEYS[1])==1 then return 1 end; " +
            "if tonumber(redis.call('GET',KEYS[2]) or '0')>=60 then return 2 end; " +
            "if tonumber(redis.call('GET',KEYS[3]) or '0')>=30 then return 2 end; " +
            "redis.call('SET',KEYS[1],'1','EX',60); " +
            "local ip=redis.call('INCR',KEYS[2]); if ip==1 then redis.call('EXPIRE',KEYS[2],3600) end; " +
            "local phone=redis.call('INCR',KEYS[3]); if phone==1 then redis.call('EXPIRE',KEYS[3],3600) end; " +
            "redis.call('HSET',KEYS[4],'code_hash',ARGV[1],'context_hash',ARGV[2],'attempts','0'); " +
            "redis.call('EXPIRE',KEYS[4],300); return 0", Long.class);
    private static final DefaultRedisScript<Long> CONSUME = new DefaultRedisScript<>(
            "if redis.call('EXISTS',KEYS[1])==0 then return 0 end; " +
            "if redis.call('HGET',KEYS[1],'code_hash')==ARGV[1] and redis.call('HGET',KEYS[1],'context_hash')==ARGV[2] " +
            "then redis.call('DEL',KEYS[1]); return 1 end; " +
            "local n=redis.call('HINCRBY',KEYS[1],'attempts',1); if n>=5 then redis.call('DEL',KEYS[1]) end; return -1", Long.class);
    private final StringRedisTemplate redis;
    private final SmsGateway gateway;

    public SmsVerificationService(StringRedisTemplate redis, SmsGateway gateway) {
        this.redis = redis;
        this.gateway = gateway;
    }

    public String mode() { return gateway.mode(); }

    public Challenge issue(String phone, String purpose, String actorId, String remoteAddress) {
        validate(phone, purpose, actorId);
        gateway.requireAvailable();
        String challengeId = UUID.randomUUID().toString();
        String code = String.format(java.util.Locale.ROOT, "%06d", RANDOM.nextInt(1_000_000));
        String key = challengeKey(challengeId);
        try {
            Long result = redis.execute(ISSUE, Arrays.asList("crossdo:sms:cooldown:" + digest(phone + ":" + purpose),
                    "crossdo:sms:ip:" + digest(remoteAddress == null ? "unknown" : remoteAddress),
                    "crossdo:sms:phone:" + digest(phone), key),
                    digest(challengeId + ":" + code), context(phone, purpose, actorId));
            if (result == null) throw unavailable();
            if (result != 0) throw new ResponseStatusException(HttpStatus.TOO_MANY_REQUESTS,
                    result == 1 ? "请等待60秒后再获取验证码" : "验证码请求过多，请稍后再试");
            try { gateway.send(phone, purpose, code, challengeId); }
            catch (RuntimeException e) { redis.delete(key); throw e; }
        } catch (DataAccessException e) { throw unavailable(); }
        return new Challenge(challengeId, "development".equals(gateway.mode()) ? code : null);
    }

    public void consume(String challengeId, String code, String phone, String purpose, String actorId) {
        validate(phone, purpose, actorId);
        gateway.requireAvailable();
        if (challengeId == null || !challengeId.matches("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}") ||
                code == null || !code.matches("[0-9]{6}")) throw invalid();
        try {
            Long result = redis.execute(CONSUME, Collections.singletonList(challengeKey(challengeId)),
                    digest(challengeId + ":" + code), context(phone, purpose, actorId));
            if (result == null) throw unavailable();
            if (result != 1) throw invalid();
        } catch (DataAccessException e) { throw unavailable(); }
    }

    private void validate(String phone, String purpose, String actorId) {
        if (phone == null || !phone.matches("1[3-9][0-9]{9}") ||
                !Arrays.asList("register", "password_reset", "phone_change").contains(purpose))
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "手机号或验证码用途不正确");
        if ("phone_change".equals(purpose) && (actorId == null || actorId.isBlank()))
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "换绑手机号需要先登录");
    }

    private String context(String phone, String purpose, String actorId) {
        return digest(gateway.mode() + "\0" + phone + "\0" + purpose + "\0" + (actorId == null ? "" : actorId));
    }
    private String challengeKey(String id) { return "crossdo:sms:challenge:" + digest(id); }
    private String digest(String value) {
        try { return Base64.getUrlEncoder().withoutPadding().encodeToString(
                MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8))); }
        catch (NoSuchAlgorithmException e) { throw new IllegalStateException(e); }
    }
    private ResponseStatusException invalid() {
        return new ResponseStatusException(HttpStatus.BAD_REQUEST, "验证码无效、已使用或已过期，请检查或重新获取");
    }
    private ResponseStatusException unavailable() {
        return new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "验证码服务暂不可用");
    }

    public static class Challenge {
        public final String challengeId;
        public final String developmentCode;
        public Challenge(String challengeId, String developmentCode) {
            this.challengeId = challengeId;
            this.developmentCode = developmentCode;
        }
    }
}
