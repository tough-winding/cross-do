package org.crossdo.web;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.auth.SessionService;
import org.crossdo.infrastructure.DbopClient;
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

class WorkflowControllerTests {
    private static final String USER_ID = "11111111-1111-4111-8111-111111111111";
    private static final String PROJECT_ID = "22222222-2222-4222-8222-222222222222";
    private DbopClient dbop;
    private SessionService sessions;
    private MockMvc mvc;
    private ObjectMapper mapper;

    @BeforeEach
    void setup() {
        dbop = mock(DbopClient.class);
        sessions = mock(SessionService.class);
        mapper = new ObjectMapper();
        when(sessions.currentUserId("Bearer valid")).thenReturn(USER_ID);
        when(dbop.call(anyString(), any(ObjectNode.class))).thenReturn(mapper.createObjectNode().put("ok", true));
        LocalValidatorFactoryBean validator = new LocalValidatorFactoryBean();
        validator.afterPropertiesSet();
        mvc = MockMvcBuilders.standaloneSetup(new WorkflowController(dbop, sessions, mapper))
                .setControllerAdvice(new ApiExceptionHandler()).setValidator(validator).build();
    }

    @Test
    void applicationIdentityComesFromSessionAndPrivilegedFieldsAreDropped() throws Exception {
        mvc.perform(post("/api/me/application").header("Authorization", "Bearer valid")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"actor_id\":\"attacker\",\"user_permission\":1,\"real_name_authentication\":1,"
                                + "\"real_name\":\"张三\",\"id_card\":\"110101199001010011\",\"user_age\":30,"
                                + "\"materials\":[\"https://files.example/medical.pdf\"]}"))
                .andExpect(status().isOk());

        ArgumentCaptor<ObjectNode> captured = ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("application/submit"), captured.capture());
        ObjectNode body = captured.getValue();
        assertEquals(USER_ID, body.path("actor_id").asText());
        assertEquals("张三", body.path("real_name").asText());
        assertFalse(body.has("user_permission"));
        assertFalse(body.has("real_name_authentication"));
    }

    @Test
    void projectSubmissionPreservesExactDecimalAndIgnoresCallerState() throws Exception {
        mvc.perform(post("/api/projects/" + PROJECT_ID + "/submit").header("Authorization", "Bearer valid")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"project_status\":0,\"actor_id\":\"someone-else\",\"donation_pool_limit\":\"1000.01\","
                                + "\"pathography\":[{\"content\":\"复查记录\",\"attachments\":[]}] }"))
                .andExpect(status().isOk());

        ArgumentCaptor<ObjectNode> captured = ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("project/submit"), captured.capture());
        assertEquals(USER_ID, captured.getValue().path("actor_id").asText());
        assertEquals(PROJECT_ID, captured.getValue().path("project_id").asText());
        assertEquals("1000.01", captured.getValue().path("donation_pool_limit").asText());
        assertFalse(captured.getValue().has("project_status"));
    }

    @Test
    void rejectsSubCentAmountsAndInvalidNestedPathography() throws Exception {
        for (String content : new String[] {
                "{\"donation_pool_limit\":\"0.001\",\"pathography\":[{\"content\":\"说明\"}]}",
                "{\"donation_pool_limit\":\"0.00\",\"pathography\":[{\"content\":\"说明\"}]}",
                "{\"donation_pool_limit\":\"10.00\",\"pathography\":[{\"content\":\" \"}]}"
        }) {
            mvc.perform(post("/api/projects/" + PROJECT_ID + "/submit").header("Authorization", "Bearer valid")
                            .contentType(MediaType.APPLICATION_JSON).content(content))
                    .andExpect(status().isBadRequest()).andExpect(jsonPath("$.status").value(400));
        }
        verifyNoInteractions(dbop);
    }

    @Test
    void rejectsUnknownReviewDecision() throws Exception {
        mvc.perform(post("/api/projects/" + PROJECT_ID + "/review").header("Authorization", "Bearer valid")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"decision\":\"force-unfreeze\",\"note\":\"test\"}"))
                .andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }

    @Test
    void unauthenticatedRequestNeverReachesDbop() throws Exception {
        when(sessions.currentUserId(isNull())).thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED, "请先登录"));
        mvc.perform(get("/api/me")).andExpect(status().isUnauthorized())
                .andExpect(jsonPath("$.status").value(401));
        verifyNoInteractions(dbop);
    }

    @Test
    void forwardsBusinessConflictWithoutExposingInternalServerErrors() throws Exception {
        when(dbop.call(eq("project/create"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.CONFLICT, "已有进行中的项目"));
        mvc.perform(post("/api/projects").header("Authorization", "Bearer valid"))
                .andExpect(status().isConflict()).andExpect(jsonPath("$.message").value("已有进行中的项目"));

        when(dbop.call(eq("project/create"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "internal upstream credentials"));
        mvc.perform(post("/api/projects").header("Authorization", "Bearer valid"))
                .andExpect(status().isServiceUnavailable()).andExpect(jsonPath("$.message").value("服务暂时不可用"));
    }

    @Test
    void malformedTargetAndJsonAreRejected() throws Exception {
        mvc.perform(post("/api/projects/not-an-id/claim").header("Authorization", "Bearer valid"))
                .andExpect(status().isBadRequest());
        mvc.perform(post("/api/me/application").header("Authorization", "Bearer valid")
                        .contentType(MediaType.APPLICATION_JSON).content("{broken}"))
                .andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }

    @Test
    void rejectsUnboundedPagination() throws Exception {
        mvc.perform(get("/api/projects?limit=101").header("Authorization", "Bearer valid"))
                .andExpect(status().isBadRequest());
        mvc.perform(get("/api/applications?offset=-1").header("Authorization", "Bearer valid"))
                .andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }

    @Test
    void projectListForwardsSummaryOptInAsBoolean() throws Exception {
        mvc.perform(get("/api/projects?summary=true&limit=6&offset=12")
                        .header("Authorization", "Bearer valid"))
                .andExpect(status().isOk());

        ArgumentCaptor<ObjectNode> captured = ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("project/list"), captured.capture());
        ObjectNode body = captured.getValue();
        assertEquals(USER_ID, body.path("actor_id").asText());
        assertEquals(6, body.path("limit").asInt());
        assertEquals(12, body.path("offset").asInt());
        assertTrue(body.path("summary").isBoolean());
        assertTrue(body.path("summary").asBoolean());
    }

    @Test
    void projectListKeepsFullResponseModeUnlessSummaryIsRequested() throws Exception {
        mvc.perform(get("/api/projects").header("Authorization", "Bearer valid"))
                .andExpect(status().isOk());
        mvc.perform(get("/api/projects?summary=false").header("Authorization", "Bearer valid"))
                .andExpect(status().isOk());

        ArgumentCaptor<ObjectNode> captured = ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop, times(2)).call(eq("project/list"), captured.capture());
        for (ObjectNode body : captured.getAllValues()) {
            assertFalse(body.has("summary"));
            assertEquals(20, body.path("limit").asInt());
            assertEquals(0, body.path("offset").asInt());
        }
    }

    @Test
    void malformedSummaryFlagNeverReachesDbop() throws Exception {
        mvc.perform(get("/api/projects?summary=all").header("Authorization", "Bearer valid"))
                .andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }
}
