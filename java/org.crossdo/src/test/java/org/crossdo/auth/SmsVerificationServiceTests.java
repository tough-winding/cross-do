package org.crossdo.auth;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.script.RedisScript;
import org.springframework.http.HttpStatus;
import org.springframework.web.server.ResponseStatusException;
import java.util.List;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;

class SmsVerificationServiceTests {
    private StringRedisTemplate redis;
    private SmsGateway gateway;
    private SmsVerificationService verification;
    private static final String PHONE="13800138000";
    @BeforeEach @SuppressWarnings("unchecked") void setup() {
        redis=mock(StringRedisTemplate.class); gateway=mock(SmsGateway.class);
        when(gateway.mode()).thenReturn("development");
        when(redis.execute(any(RedisScript.class),anyList(),anyString(),anyString())).thenReturn(0L);
        verification=new SmsVerificationService(redis,gateway);
    }
    @Test @SuppressWarnings({"unchecked","rawtypes"}) void developmentReturnsCodeButRedisOnlyGetsHashes() {
        SmsVerificationService.Challenge challenge=verification.issue(PHONE,"register",null,"127.0.0.1");
        assertTrue(challenge.developmentCode.matches("[0-9]{6}"));
        ArgumentCaptor<List> keys=ArgumentCaptor.forClass(List.class);
        ArgumentCaptor<String> code=ArgumentCaptor.forClass(String.class), context=ArgumentCaptor.forClass(String.class);
        verify(redis).execute(any(RedisScript.class),keys.capture(),code.capture(),context.capture());
        assertFalse(keys.getValue().toString().contains(PHONE));
        assertNotEquals(challenge.developmentCode,code.getValue()); assertEquals(43,code.getValue().length());
        assertFalse(context.getValue().contains(PHONE));
        verify(gateway).send(PHONE,"register",challenge.developmentCode,challenge.challengeId);
    }
    @Test void gatewayNeverReturnsCode() {
        when(gateway.mode()).thenReturn("gateway");
        assertNull(verification.issue(PHONE,"password_reset",null,"127.0.0.1").developmentCode);
        verify(gateway).send(eq(PHONE),eq("password_reset"),matches("[0-9]{6}"),anyString());
    }
    @Test @SuppressWarnings("unchecked") void rateLimitPreventsSending() {
        when(redis.execute(any(RedisScript.class),anyList(),anyString(),anyString())).thenReturn(1L);
        assertEquals(429,assertThrows(ResponseStatusException.class,
                () -> verification.issue(PHONE,"register",null,"127.0.0.1")).getRawStatusCode());
        verify(gateway,never()).send(anyString(),anyString(),anyString(),anyString());
    }
    @Test void disabledModeDoesNotCreateChallenge() {
        doThrow(new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE,"disabled")).when(gateway).requireAvailable();
        assertEquals(503,assertThrows(ResponseStatusException.class,
                () -> verification.issue(PHONE,"register",null,"127.0.0.1")).getRawStatusCode());
        verifyNoInteractions(redis);
    }
    @Test @SuppressWarnings("unchecked") void consumedOrRejectedChallengeCannotAuthorize() {
        when(redis.execute(any(RedisScript.class),anyList(),anyString(),anyString())).thenReturn(1L,0L,-1L);
        String challenge="11111111-1111-4111-8111-111111111111";
        verification.consume(challenge,"123456",PHONE,"register",null);
        for(int n=0;n<2;n++) assertEquals(400,assertThrows(ResponseStatusException.class,
                () -> verification.consume(challenge,"123456",PHONE,"register",null)).getRawStatusCode());
    }
    @Test void phoneChangeRequiresBoundActor() {
        assertEquals(401,assertThrows(ResponseStatusException.class,
                () -> verification.issue(PHONE,"phone_change",null,"127.0.0.1")).getRawStatusCode());
        verifyNoInteractions(redis);
    }
}
