package org.crossdo.auth;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.infrastructure.DbopClient;
import org.crossdo.web.ApiExceptionHandler;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
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
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

class AuthControllerTests {
    private static final String USER_ID = "11111111-1111-4111-8111-111111111111";
    private static final String PHONE = "13800138000";
    private static final String PASSWORD = "review-password-123";
    private static final BCryptPasswordEncoder ENCODER = new BCryptPasswordEncoder(4);
    private static final String HASH = ENCODER.encode(PASSWORD);

    private final ObjectMapper mapper = new ObjectMapper();
    private DbopClient dbop;
    private SessionService sessions;
    private SmsVerificationService verification;
    private MockMvc mvc;

    @BeforeEach
    void setup() {
        dbop = mock(DbopClient.class);
        sessions = mock(SessionService.class);
        verification = mock(SmsVerificationService.class);
        LocalValidatorFactoryBean validator = new LocalValidatorFactoryBean();
        validator.afterPropertiesSet();
        mvc = MockMvcBuilders.standaloneSetup(new AuthController(dbop, sessions, mapper, verification))
                .setControllerAdvice(new ApiExceptionHandler()).setValidator(validator).build();
    }

    @Test
    void registrationHashesPasswordAndForwardsOnlyRegistrationFields() throws Exception {
        when(dbop.call(eq("register"), any(ObjectNode.class)))
                .thenReturn(mapper.createObjectNode().put("user_id", USER_ID).put("user_status", 1));
        ObjectNode request = registration(PASSWORD, 3);
        request.put("actor_id", "forged-identity").put("real_name_authentication", 1).put("user_status", 0);

        mvc.perform(post("/api/auth/register").contentType(MediaType.APPLICATION_JSON).content(request.toString()))
                .andExpect(status().isCreated()).andExpect(jsonPath("$.user_id").value(USER_ID))
                .andExpect(jsonPath("$.password").doesNotExist()).andExpect(jsonPath("$.user_password").doesNotExist());

        ArgumentCaptor<ObjectNode> body = ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("register"), body.capture());
        assertEquals(5, body.getValue().size());
        assertTrue(body.getValue().path("phone_verified").asBoolean());
        verify(verification).consume(anyString(),eq("123456"),eq(PHONE),eq("register"),isNull());
        assertEquals(PHONE, body.getValue().path("phone_number").asText());
        assertEquals(3, body.getValue().path("user_permission").asInt());
        String hash = body.getValue().path("password_hash").asText();
        assertTrue(ENCODER.matches(PASSWORD, hash));
        assertNotEquals(PASSWORD, hash);
        assertFalse(body.getValue().has("password"));
        assertFalse(body.getValue().has("actor_id"));
        assertFalse(body.getValue().has("real_name_authentication"));
        verifyNoInteractions(sessions);
    }

    @Test
    void privilegedRolesCannotBeSelfRegistered() throws Exception {
        for (int role : new int[] {1, 6, 7}) {
            mvc.perform(post("/api/auth/register").contentType(MediaType.APPLICATION_JSON)
                            .content(registration(PASSWORD, role).toString()))
                    .andExpect(status().isBadRequest());
        }
        verifyNoInteractions(dbop, sessions);
    }

    @Test
    void successfulLoginUpdatesLoginTimeBeforeCreatingSessionAndNeverReturnsCredentials() throws Exception {
        when(dbop.call(eq("credentials"), any(ObjectNode.class))).thenReturn(credentials(0));
        when(dbop.call(eq("account/login"), any(ObjectNode.class))).thenReturn(
                mapper.createObjectNode().put("user_id", USER_ID).put("user_status", 0).put("user_name", "患者"));
        when(sessions.create(USER_ID, "0")).thenReturn("opaque-session-token");
        when(sessions.expiresIn()).thenReturn(7200L);

        mvc.perform(post("/api/auth/login").contentType(MediaType.APPLICATION_JSON).content(login(PASSWORD).toString()))
                .andExpect(status().isOk()).andExpect(jsonPath("$.token").value("opaque-session-token"))
                .andExpect(jsonPath("$.token_type").value("Bearer")).andExpect(jsonPath("$.expires_in").value(7200))
                .andExpect(jsonPath("$.user.user_id").value(USER_ID))
                .andExpect(jsonPath("$.user.user_password").doesNotExist())
                .andExpect(jsonPath("$.password_hash").doesNotExist());

        InOrder order = inOrder(dbop, sessions);
        order.verify(sessions).checkLoginLimit(PHONE);
        order.verify(dbop).call(eq("credentials"), any(ObjectNode.class));
        order.verify(dbop).call(eq("account/login"), argThat(body -> USER_ID.equals(body.path("actor_id").asText())));
        order.verify(sessions).loginSucceeded(PHONE);
        order.verify(sessions).create(USER_ID, "0");
        verify(sessions, never()).loginFailed(anyString());
    }

    @Test
    void incorrectPasswordNeverCreatesSessionOrUpdatesLoginTime() throws Exception {
        when(dbop.call(eq("credentials"), any(ObjectNode.class))).thenReturn(credentials(0));
        mvc.perform(post("/api/auth/login").contentType(MediaType.APPLICATION_JSON)
                        .content(login("incorrect-password").toString()))
                .andExpect(status().isUnauthorized()).andExpect(jsonPath("$.message").value("手机号或密码不正确"));
        verify(sessions).loginFailed(PHONE);
        verify(sessions, never()).create(anyString(), anyString());
        verify(sessions, never()).loginSucceeded(anyString());
        verify(dbop, never()).call(eq("account/login"), any(ObjectNode.class));
    }

    @Test
    void unknownPhoneUsesSameUnauthorizedResponse() throws Exception {
        when(dbop.call(eq("credentials"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.NOT_FOUND, "Account not found"));
        mvc.perform(post("/api/auth/login").contentType(MediaType.APPLICATION_JSON).content(login(PASSWORD).toString()))
                .andExpect(status().isUnauthorized()).andExpect(jsonPath("$.message").value("手机号或密码不正确"));
        verify(sessions).loginFailed(PHONE);
        verify(sessions, never()).create(anyString(), anyString());
    }

    @Test
    void frozenAccountCannotCreateSession() throws Exception {
        when(dbop.call(eq("credentials"), any(ObjectNode.class))).thenReturn(credentials(10));
        mvc.perform(post("/api/auth/login").contentType(MediaType.APPLICATION_JSON).content(login(PASSWORD).toString()))
                .andExpect(status().isForbidden());
        verify(sessions, never()).create(anyString(), anyString());
        verify(sessions, never()).loginSucceeded(anyString());
        verify(dbop, never()).call(eq("account/login"), any(ObjectNode.class));
    }

    @Test
    void passwordsAboveBcryptUtf8ByteLimitAreRejectedBeforeAnyServiceCall() throws Exception {
        String password = "密".repeat(25); // 25 characters, 75 UTF-8 bytes.
        mvc.perform(post("/api/auth/register").contentType(MediaType.APPLICATION_JSON)
                        .content(registration(password, 3).toString()))
                .andExpect(status().isBadRequest());
        mvc.perform(post("/api/auth/login").contentType(MediaType.APPLICATION_JSON).content(login(password).toString()))
                .andExpect(status().isBadRequest());
        verifyNoInteractions(dbop, sessions);
    }

    @Test
    void loginRateLimitStopsCredentialLookup() throws Exception {
        doThrow(new ResponseStatusException(HttpStatus.TOO_MANY_REQUESTS, "登录失败次数过多，请稍后再试"))
                .when(sessions).checkLoginLimit(PHONE);
        mvc.perform(post("/api/auth/login").contentType(MediaType.APPLICATION_JSON).content(login(PASSWORD).toString()))
                .andExpect(status().isTooManyRequests());
        verifyNoInteractions(dbop);
        verify(sessions, never()).create(anyString(), anyString());
    }


    @Test
    void registrationWithoutOtpOrWithRejectedOtpCannotReachDbop() throws Exception {
        ObjectNode missing = registration(PASSWORD, 3);
        missing.remove("code");
        mvc.perform(post("/api/auth/register").contentType(MediaType.APPLICATION_JSON).content(missing.toString()))
                .andExpect(status().isBadRequest());
        doThrow(new ResponseStatusException(HttpStatus.BAD_REQUEST, "验证码无效"))
                .when(verification).consume(anyString(), anyString(), anyString(), eq("register"), isNull());
        mvc.perform(post("/api/auth/register").contentType(MediaType.APPLICATION_JSON)
                .content(registration(PASSWORD, 3).toString())).andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }

    @Test
    void frozenAccountStateBlocksLoginWithoutDestroyingReviewStatus() throws Exception {
        ObjectNode account = credentials(3);
        account.put("account_state", "frozen");
        when(dbop.call(eq("credentials"), any(ObjectNode.class))).thenReturn(account);
        mvc.perform(post("/api/auth/login").contentType(MediaType.APPLICATION_JSON).content(login(PASSWORD).toString()))
                .andExpect(status().isForbidden());
        verify(sessions, never()).create(anyString(), anyString());
    }

    @Test
    void loginCannotIssueASessionIfTheVerifiedCredentialsWereRevokedMeanwhile() throws Exception {
        ObjectNode account = credentials(0).put("session_version", 9);
        when(dbop.call(eq("credentials"), any(ObjectNode.class))).thenReturn(account);
        when(dbop.call(eq("account/login"), any(ObjectNode.class)))
                .thenReturn(mapper.createObjectNode().put("user_id", USER_ID));
        when(sessions.create(USER_ID, "9"))
                .thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED, "revoked"));
        mvc.perform(post("/api/auth/login").contentType(MediaType.APPLICATION_JSON).content(login(PASSWORD).toString()))
                .andExpect(status().isUnauthorized()).andExpect(jsonPath("$.token").doesNotExist());
        verify(sessions).create(USER_ID, "9");
    }

    private ObjectNode credentials(int status) {
        return mapper.createObjectNode().put("user_id", USER_ID).put("user_password", HASH)
                .put("account_state", "active").put("session_version", 0).put("user_status", status).put("user_permission", 3).put("real_name_authentication", 1);
    }

    private ObjectNode login(String password) {
        return mapper.createObjectNode().put("phone_number", PHONE).put("password", password);
    }

    private ObjectNode registration(String password, int role) {
        return login(password).put("user_name", "患者").put("user_permission", role)
                .put("challenge_id", "22222222-2222-4222-8222-222222222222").put("code", "123456");
    }
}
