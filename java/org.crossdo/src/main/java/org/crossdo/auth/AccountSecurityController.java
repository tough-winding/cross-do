package org.crossdo.auth;

import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.infrastructure.DbopClient;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

import javax.servlet.http.HttpServletRequest;
import javax.validation.Valid;
import javax.validation.constraints.NotBlank;
import javax.validation.constraints.Pattern;
import javax.validation.constraints.Size;

@RestController
@RequestMapping("/api")
public class AccountSecurityController {
    private final DbopClient dbop;
    private final SessionService sessions;
    private final SmsVerificationService verification;
    private final ObjectMapper mapper;
    private final BCryptPasswordEncoder passwords = new BCryptPasswordEncoder(12);

    public AccountSecurityController(DbopClient dbop, SessionService sessions,
                                     SmsVerificationService verification, ObjectMapper mapper) {
        this.dbop = dbop;
        this.sessions = sessions;
        this.verification = verification;
        this.mapper = mapper;
    }

    @GetMapping("/auth/settings")
    public JsonNode settings() {
        return mapper.createObjectNode().put("sms_mode", verification.mode()).put("phone_verification_required", true);
    }

    @PostMapping("/auth/verification-codes")
    public JsonNode requestCode(@Valid @RequestBody CodeRequest request,
                               @RequestHeader(value = "Authorization", required = false) String authorization,
                               HttpServletRequest servletRequest) {
        String actor = "phone_change".equals(request.purpose) ? sessions.currentUserId(authorization) : null;
        // Remote address is provided by the servlet container, never trust a caller's X-Forwarded-For header.
        SmsVerificationService.Challenge challenge = verification.issue(request.phoneNumber, request.purpose,
                actor, servletRequest.getRemoteAddr());
        ObjectNode result = mapper.createObjectNode().put("challenge_id", challenge.challengeId)
                .put("expires_in", 300).put("resend_after", 60);
        if (challenge.developmentCode != null) result.put("development_code", challenge.developmentCode);
        return result;
    }

    @PostMapping("/auth/password-reset")
    public ResponseEntity<Void> resetPassword(@Valid @RequestBody PasswordReset request) {
        AuthController.checkPassword(request.password);
        verification.consume(request.challengeId, request.code, request.phoneNumber, "password_reset", null);
        dbop.call("security/reset-password", mapper.createObjectNode().put("phone_number", request.phoneNumber)
                .put("password_hash", passwords.encode(request.password)));
        afterCommittedChange(() -> sessions.loginSucceeded(request.phoneNumber));
        return ResponseEntity.noContent().build();
    }

    @PostMapping("/me/phone")
    public ResponseEntity<Void> changePhone(@RequestHeader(value = "Authorization", required = false) String authorization,
                                          @Valid @RequestBody PasswordReset request) {
        AuthController.checkPassword(request.password);
        String actor = sessions.currentUserId(authorization);
        // Limit old-password guessing using the same account-keyed limiter as login.
        String limiter = "phone-change:" + actor;
        sessions.checkLoginLimit(limiter);
        JsonNode credentials = dbop.call("security/credentials", mapper.createObjectNode().put("actor_id", actor));
        String hash = credentials.path("user_password").asText("");
        if (!passwords.matches(request.password, hash)) {
            sessions.loginFailed(limiter);
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "当前密码不正确");
        }
        verification.consume(request.challengeId, request.code, request.phoneNumber, "phone_change", actor);
        dbop.call("security/change-phone", mapper.createObjectNode().put("actor_id", actor)
                .put("phone_number", request.phoneNumber).put("expected_password_hash", hash)
                .put("expected_session_version", credentials.path("session_version").asText()));
        afterCommittedChange(() -> sessions.loginSucceeded(limiter));
        afterCommittedChange(() -> sessions.logout(authorization));
        return ResponseEntity.noContent().build();
    }

    private void afterCommittedChange(Runnable cleanup) {
        // The DB version already revoked every old session. An optional Redis
        // cleanup outage must not turn a committed credential change into 503.
        try { cleanup.run(); }
        catch (ResponseStatusException e) {
            if (e.getStatus() != HttpStatus.SERVICE_UNAVAILABLE) throw e;
        }
    }

    public static class CodeRequest {
        @JsonProperty("phone_number") @NotBlank @Pattern(regexp = "1[3-9][0-9]{9}")
        public String phoneNumber;
        @NotBlank @Pattern(regexp = "register|password_reset|phone_change")
        public String purpose;
    }

    public static class PasswordReset {
        @JsonProperty("phone_number") @NotBlank @Pattern(regexp = "1[3-9][0-9]{9}")
        public String phoneNumber;
        @JsonProperty("challenge_id") @NotBlank
        @Pattern(regexp = "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}")
        public String challengeId;
        @NotBlank @Pattern(regexp = "[0-9]{6}")
        public String code;
        @NotBlank @Size(min = 8, max = 72)
        public String password;
    }
}
