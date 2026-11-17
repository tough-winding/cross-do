package org.crossdo.auth;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.infrastructure.DbopClient;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

import javax.validation.Valid;
import javax.validation.constraints.NotBlank;
import javax.validation.constraints.NotNull;
import javax.validation.constraints.Pattern;
import javax.validation.constraints.Size;
import com.fasterxml.jackson.annotation.JsonProperty;
import java.nio.charset.StandardCharsets;

@RestController
@RequestMapping("/api/auth")
public class AuthController {
    private final DbopClient dbop;
    private final SessionService sessions;
    private final ObjectMapper mapper;
    private final SmsVerificationService verification;
    private final BCryptPasswordEncoder passwords = new BCryptPasswordEncoder(12);
    private final String dummyHash = passwords.encode("no-account-placeholder");

    public AuthController(DbopClient dbop, SessionService sessions, ObjectMapper mapper, SmsVerificationService verification) {
        this.dbop = dbop;
        this.sessions = sessions;
        this.mapper = mapper;
        this.verification = verification;
    }

    @PostMapping("/register")
    public ResponseEntity<JsonNode> register(@Valid @RequestBody Registration request) {
        checkPassword(request.password);
        if (request.userPermission != 2 && request.userPermission != 3 && request.userPermission != 5)
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "注册角色仅允许捐助者、患者或志愿者");
        verification.consume(request.challengeId, request.code, request.phoneNumber, "register", null);
        ObjectNode body = mapper.createObjectNode()
                .put("phone_verified", true).put("phone_number", request.phoneNumber).put("user_name", request.userName)
                .put("password_hash", passwords.encode(request.password))
                .put("user_permission", request.userPermission);
        return ResponseEntity.status(HttpStatus.CREATED).body(dbop.call("register", body));
    }

    @PostMapping("/login")
    public JsonNode login(@Valid @RequestBody Login request) {
        checkPassword(request.password);
        sessions.checkLoginLimit(request.phoneNumber);
        JsonNode account = null;
        try {
            account = dbop.call("credentials", mapper.createObjectNode().put("phone_number", request.phoneNumber));
        } catch (ResponseStatusException e) {
            if (e.getStatus() != HttpStatus.NOT_FOUND) throw e;
        }
        String stored = account == null ? dummyHash : account.path("user_password").asText("");
        if (!passwords.matches(request.password, stored) || account == null) {
            sessions.loginFailed(request.phoneNumber);
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "手机号或密码不正确");
        }
        int status = account.path("user_status").asInt(13);
        if (status < 0 || status >= 8 || !"active".equals(account.path("account_state").asText("")))
            throw new ResponseStatusException(HttpStatus.FORBIDDEN, "账号当前不可登录");
        String id = account.path("user_id").asText();
        JsonNode profile = dbop.call("account/login", mapper.createObjectNode().put("actor_id", id));
        sessions.loginSucceeded(request.phoneNumber);
        String token = sessions.create(id, account.path("session_version").asText(""));
        ObjectNode result = mapper.createObjectNode().put("token", token)
                .put("token_type", "Bearer").put("expires_in", sessions.expiresIn());
        result.set("user", profile);
        return result;
    }

    @PostMapping("/logout")
    public ResponseEntity<Void> logout(@RequestHeader(value = "Authorization", required = false) String authorization) {
        sessions.logout(authorization);
        return ResponseEntity.noContent().build();
    }

    static void checkPassword(String password) {
        if (password.getBytes(StandardCharsets.UTF_8).length > 72)
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "密码 UTF-8 长度不能超过72字节");
    }

    public static class Login {
        @JsonProperty("phone_number")
        @NotBlank @Pattern(regexp = "1[3-9][0-9]{9}")
        public String phoneNumber;
        @NotBlank @Size(min = 8, max = 72)
        public String password;
    }

    public static class Registration extends Login {
        @JsonProperty("challenge_id") @NotBlank
        @Pattern(regexp = "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}")
        public String challengeId;
        @NotBlank @Pattern(regexp = "[0-9]{6}")
        public String code;
        @JsonProperty("user_name") @NotBlank @Size(max = 24)
        public String userName;
        @JsonProperty("user_permission") @NotNull
        public Integer userPermission;
    }
}
