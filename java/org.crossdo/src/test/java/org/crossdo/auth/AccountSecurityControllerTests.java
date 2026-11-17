package org.crossdo.auth;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.infrastructure.DbopClient;
import org.crossdo.web.ApiExceptionHandler;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import org.springframework.validation.beanvalidation.LocalValidatorFactoryBean;
import org.springframework.web.server.ResponseStatusException;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.*;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

class AccountSecurityControllerTests {
    private static final String USER = "11111111-1111-4111-8111-111111111111";
    private static final String CHALLENGE = "22222222-2222-4222-8222-222222222222";
    private static final String PHONE = "13900139000";
    private static final String PASSWORD = "current-password-123";
    private final ObjectMapper mapper = new ObjectMapper();
    private DbopClient dbop;
    private SessionService sessions;
    private SmsVerificationService verification;
    private MockMvc mvc;
    @BeforeEach void setup() {
        dbop = mock(DbopClient.class); sessions = mock(SessionService.class);
        verification = mock(SmsVerificationService.class);
        when(sessions.currentUserId("Bearer valid")).thenReturn(USER);
        LocalValidatorFactoryBean validator = new LocalValidatorFactoryBean(); validator.afterPropertiesSet();
        mvc = MockMvcBuilders.standaloneSetup(new AccountSecurityController(dbop,sessions,verification,mapper))
                .setControllerAdvice(new ApiExceptionHandler()).setValidator(validator).build();
    }
    @Test void developmentCodeIsAbsentForGatewayDelivery() throws Exception {
        when(verification.issue(eq(PHONE),eq("register"),isNull(),anyString()))
                .thenReturn(new SmsVerificationService.Challenge(CHALLENGE,null));
        mvc.perform(post("/api/auth/verification-codes").contentType(MediaType.APPLICATION_JSON)
                .content("{\"phone_number\":\""+PHONE+"\",\"purpose\":\"register\"}"))
                .andExpect(status().isOk()).andExpect(jsonPath("$.development_code").doesNotExist())
                .andExpect(jsonPath("$.expires_in").value(300));
    }
    @Test void phoneChallengeBindsActorAndIgnoresUntrustedForwardedIp() throws Exception {
        when(verification.issue(PHONE,"phone_change",USER,"127.0.0.1"))
                .thenReturn(new SmsVerificationService.Challenge(CHALLENGE,"123456"));
        mvc.perform(post("/api/auth/verification-codes").header("Authorization","Bearer valid")
                .header("X-Forwarded-For","1.2.3.4").contentType(MediaType.APPLICATION_JSON)
                .content("{\"phone_number\":\""+PHONE+"\",\"purpose\":\"phone_change\",\"actor_id\":\"forged\"}"))
                .andExpect(status().isOk());
        verify(verification).issue(PHONE,"phone_change",USER,"127.0.0.1");
    }
    @Test void resetUsesHashedPasswordAndDoesNotFailAfterCommittedChange() throws Exception {
        doThrow(new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE,"Redis down"))
                .when(sessions).loginSucceeded(PHONE);
        mvc.perform(post("/api/auth/password-reset").contentType(MediaType.APPLICATION_JSON).content(change().toString()))
                .andExpect(status().isNoContent());
        verify(verification).consume(CHALLENGE,"123456",PHONE,"password_reset",null);
        ArgumentCaptor<ObjectNode> body=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("security/reset-password"),body.capture());
        assertEquals(2,body.getValue().size());
        assertTrue(new BCryptPasswordEncoder().matches(PASSWORD,body.getValue().path("password_hash").asText()));
    }
    @Test void wrongPasswordCannotConsumeChallengeOrChangePhone() throws Exception {
        when(dbop.call(eq("security/credentials"),any(ObjectNode.class))).thenReturn(mapper.createObjectNode()
                .put("user_password",new BCryptPasswordEncoder(4).encode("a-different-password")).put("session_version",7));
        mvc.perform(post("/api/me/phone").header("Authorization","Bearer valid").contentType(MediaType.APPLICATION_JSON)
                .content(change().toString())).andExpect(status().isBadRequest());
        verifyNoInteractions(verification);
        verify(dbop,never()).call(eq("security/change-phone"),any(ObjectNode.class));
    }
    @Test void phoneChangeUsesVersionAndIgnoresOptionalCleanupOutage() throws Exception {
        String hash=new BCryptPasswordEncoder(4).encode(PASSWORD);
        when(dbop.call(eq("security/credentials"),any(ObjectNode.class))).thenReturn(mapper.createObjectNode()
                .put("user_password",hash).put("session_version",7));
        doThrow(new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE,"Redis down")).when(sessions).logout("Bearer valid");
        mvc.perform(post("/api/me/phone").header("Authorization","Bearer valid").contentType(MediaType.APPLICATION_JSON)
                .content(change().put("actor_id","forged").toString())).andExpect(status().isNoContent());
        verify(verification).consume(CHALLENGE,"123456",PHONE,"phone_change",USER);
        verify(dbop).call(eq("security/change-phone"),argThat(body -> USER.equals(body.path("actor_id").asText())
                && "7".equals(body.path("expected_session_version").asText())
                && hash.equals(body.path("expected_password_hash").asText())));
    }
    private ObjectNode change() {
        return mapper.createObjectNode().put("phone_number",PHONE).put("challenge_id",CHALLENGE)
                .put("code","123456").put("password",PASSWORD);
    }
}
