package org.crossdo.operations;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.auth.SessionService;
import org.crossdo.infrastructure.DbopClient;
import org.crossdo.web.ApiExceptionHandler;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import org.springframework.validation.beanvalidation.LocalValidatorFactoryBean;
import org.springframework.web.server.ResponseStatusException;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.*;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

class OperationsControllerTests {
    private static final String ACTOR="11111111-1111-4111-8111-111111111111";
    private static final String TARGET="22222222-2222-4222-8222-222222222222";
    private DbopClient dbop;
    private SessionService sessions;
    private MockMvc mvc;
    private ObjectMapper mapper;

    @BeforeEach
    void setup() {
        dbop=mock(DbopClient.class); sessions=mock(SessionService.class); mapper=new ObjectMapper();
        when(sessions.currentUserId("Bearer valid")).thenReturn(ACTOR);
        when(dbop.call(anyString(),any(ObjectNode.class))).thenReturn(mapper.createObjectNode().put("ok",true));
        LocalValidatorFactoryBean validator=new LocalValidatorFactoryBean(); validator.afterPropertiesSet();
        mvc=MockMvcBuilders.standaloneSetup(new OperationsController(dbop,sessions,mapper))
                .setControllerAdvice(new ApiExceptionHandler()).setValidator(validator).build();
    }
    @Test
    void certificationUsesSessionIdentityAndDiscardsPrivilegeInjection() throws Exception {
        mvc.perform(post("/api/admin/users/"+TARGET+"/certification").header("Authorization","Bearer valid")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"approved\":true,\"note\":\"线下已核验\",\"actor_id\":\"attacker\",\"user_permission\":1}"))
                .andExpect(status().isOk());
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("ops/certification"),captured.capture());
        assertEquals(ACTOR,captured.getValue().path("actor_id").asText());
        assertEquals(TARGET,captured.getValue().path("user_id").asText());
        assertFalse(captured.getValue().has("user_permission"));
    }
    @Test
    void blankAdminReasonsAndInvalidDecisionsNeverReachDatabase() throws Exception {
        mvc.perform(post("/api/admin/users/"+TARGET+"/state").header("Authorization","Bearer valid")
                .contentType(MediaType.APPLICATION_JSON).content("{\"state\":\"frozen\",\"note\":\" \"}"))
                .andExpect(status().isBadRequest());
        mvc.perform(post("/api/admin/projects/"+TARGET+"/closure/review").header("Authorization","Bearer valid")
                .contentType(MediaType.APPLICATION_JSON).content("{\"decision\":\"skip-funds\",\"note\":\"跳过资金检查\"}"))
                .andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }
    @Test
    void releaseRequiresExplicitNullVolunteerInDatabasePayload() throws Exception {
        mvc.perform(post("/api/admin/tasks/project/"+TARGET+"/reassign").header("Authorization","Bearer valid")
                .contentType(MediaType.APPLICATION_JSON).content("{\"volunteer_id\":null,\"note\":\"超时释放\"}"))
                .andExpect(status().isOk());
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("ops/reassign"),captured.capture());
        assertTrue(captured.getValue().has("volunteer_id"));
        assertTrue(captured.getValue().get("volunteer_id").isNull());
        assertEquals("project",captured.getValue().path("kind").asText());
    }
    @Test
    void caseRequiresExactlyOneTargetAndAppealRequiresParent() throws Exception {
        for(String content:new String[]{
                "{\"kind\":\"report\",\"reason\":\"问题\",\"evidence\":[]}",
                "{\"kind\":\"report\",\"project_id\":\""+TARGET+"\",\"target_user_id\":\""+ACTOR+"\",\"reason\":\"问题\"}",
                "{\"kind\":\"appeal\",\"reason\":\"请求复核\"}"}) {
            mvc.perform(post("/api/cases").header("Authorization","Bearer valid")
                    .contentType(MediaType.APPLICATION_JSON).content(content)).andExpect(status().isBadRequest());
        }
        verifyNoInteractions(dbop);
    }
    @Test
    void caseAcceptsOwnedFileReferenceButNotLocalServerPath() throws Exception {
        mvc.perform(post("/api/cases").header("Authorization","Bearer valid").contentType(MediaType.APPLICATION_JSON)
                .content("{\"kind\":\"report\",\"project_id\":\""+TARGET+"\",\"reason\":\"待核查\",\"evidence\":[\"/api/files/"+TARGET+"\"]}"))
                .andExpect(status().isOk());
        reset(dbop);
        mvc.perform(post("/api/cases").header("Authorization","Bearer valid").contentType(MediaType.APPLICATION_JSON)
                .content("{\"kind\":\"report\",\"project_id\":\""+TARGET+"\",\"reason\":\"待核查\",\"evidence\":[\"file:///etc/passwd\"]}"))
                .andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }
    @Test
    void authenticationAndBusinessAuthorizationFailuresArePreserved() throws Exception {
        when(sessions.currentUserId(isNull())).thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED,"请先登录"));
        mvc.perform(get("/api/admin/overview")).andExpect(status().isUnauthorized());
        verifyNoInteractions(dbop);
        when(dbop.call(eq("ops/overview"),any(ObjectNode.class))).thenThrow(new ResponseStatusException(HttpStatus.FORBIDDEN,"管理员权限不足"));
        mvc.perform(get("/api/admin/overview").header("Authorization","Bearer valid")).andExpect(status().isForbidden());
    }
    @Test
    void notificationOwnershipAlwaysUsesAuthenticatedIdentity() throws Exception {
        mvc.perform(post("/api/notifications/"+TARGET+"/read").header("Authorization","Bearer valid"))
                .andExpect(status().isOk());
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("ops/notification-read"),captured.capture());
        assertEquals(ACTOR,captured.getValue().path("actor_id").asText());
        assertEquals(TARGET,captured.getValue().path("id").asText());
    }
    @Test
    void pendingMoneyConflictIsReturnedWithoutSilentlyClosing() throws Exception {
        when(dbop.call(eq("ops/closure-review"),any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.CONFLICT,"资金尚未结清"));
        mvc.perform(post("/api/admin/projects/"+TARGET+"/closure/review").header("Authorization","Bearer valid")
                .contentType(MediaType.APPLICATION_JSON).content("{\"decision\":\"approve\",\"note\":\"核对结项\"}"))
                .andExpect(status().isConflict());
    }
    @Test
    void anonymousClosureReadOmitsActorAndPreservesPrivateProjectDenial() throws Exception {
        mvc.perform(get("/api/projects/"+TARGET+"/closure")).andExpect(status().isOk());
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("ops/closure-get"),captured.capture());
        assertEquals(TARGET,captured.getValue().path("project_id").asText());
        assertFalse(captured.getValue().has("actor_id"));
        verifyNoInteractions(sessions);
        when(dbop.call(eq("ops/closure-get"),any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.FORBIDDEN,"尚未公开"));
        mvc.perform(get("/api/projects/"+TARGET+"/closure")).andExpect(status().isForbidden());
    }
    @Test
    void providedInvalidAuthorizationNeverBecomesAnonymousClosureRead() throws Exception {
        when(sessions.currentUserId("Bearer invalid"))
                .thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED,"登录已失效"));
        when(sessions.currentUserId(""))
                .thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED,"请先登录"));
        mvc.perform(get("/api/projects/"+TARGET+"/closure").header("Authorization","Bearer invalid"))
                .andExpect(status().isUnauthorized());
        mvc.perform(get("/api/projects/"+TARGET+"/closure").header("Authorization",""))
                .andExpect(status().isUnauthorized());
        verifyNoInteractions(dbop);
        mvc.perform(get("/api/projects/"+TARGET+"/closure").header("Authorization","Bearer valid"))
                .andExpect(status().isOk());
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("ops/closure-get"),captured.capture());
        assertEquals(ACTOR,captured.getValue().path("actor_id").asText());
    }
    @Test
    void paginationIsBoundedEvenWithoutMethodValidationProxy() throws Exception {
        mvc.perform(get("/api/admin/users?limit=101").header("Authorization","Bearer valid")).andExpect(status().isBadRequest());
        mvc.perform(get("/api/notifications?offset=1000001").header("Authorization","Bearer valid")).andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }
}
