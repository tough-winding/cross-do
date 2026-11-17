package org.crossdo.finance;

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

class FinanceControllerTests {
    private static final String ACTOR="11111111-1111-4111-8111-111111111111";
    private static final String ID="22222222-2222-4222-8222-222222222222";
    private DbopClient dbop;private SessionService sessions;private MockMvc mvc;private ObjectMapper mapper;
    @BeforeEach void setup(){
        dbop=mock(DbopClient.class);sessions=mock(SessionService.class);mapper=new ObjectMapper();
        when(sessions.currentUserId("Bearer valid")).thenReturn(ACTOR);
        when(sessions.currentUserId(isNull())).thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED,"请先登录"));
        when(dbop.call(anyString(),any(ObjectNode.class))).thenReturn(record("pending"));
        configure("development");
    }
    private ObjectNode record(String status){
        ObjectNode record=mapper.createObjectNode().put("id",ID).put("project_id",ID)
                .put("amount","10.25").put("mode","development").put("status",status);
        record.putArray("refunds");return record;
    }
    private void configure(String mode){
        LocalValidatorFactoryBean validator=new LocalValidatorFactoryBean();validator.afterPropertiesSet();
        PaymentGateway gateway=new PaymentGateway(mapper,mode,"","","");
        mvc=MockMvcBuilders.standaloneSetup(new FinanceController(dbop,sessions,mapper,gateway))
                .setControllerAdvice(new ApiExceptionHandler()).setValidator(validator).build();
    }
    private String donation(String amount){return "{\"amount\":\""+amount+"\",\"idempotency_key\":\""+ID+"\",\"project_id\":\""+ID+"\"}";}
    @Test void donationUsesSessionActorAndConfiguredModeIgnoringInjectedPrivileges()throws Exception{
        String body=donation("10.25").replace("}",",\"actor_id\":\"attacker\",\"mode\":\"gateway\",\"admin\":true}");
        mvc.perform(post("/api/donations").header("Authorization","Bearer valid").contentType(MediaType.APPLICATION_JSON).content(body))
                .andExpect(status().isOk()).andExpect(jsonPath("$.mode").value("development"));
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("finance/order_create"),captured.capture());
        assertEquals(ACTOR,captured.getValue().path("actor_id").asText());
        assertEquals("development",captured.getValue().path("mode").asText());
        assertFalse(captured.getValue().has("admin"));
    }
    @Test void invalidMoneyNeverReachesDatabase()throws Exception{
        for(String amount:new String[]{"0","1.001","-1","1e2","100000000","NaN"})
            mvc.perform(post("/api/donations").header("Authorization","Bearer valid").contentType(MediaType.APPLICATION_JSON).content(donation(amount)))
                    .andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }
    @Test void evidenceElementValidationReturns400RatherThanAccessorFailure()throws Exception{
        String body="{\"amount\":\"1\",\"idempotency_key\":\""+ID+"\",\"purpose\":\"医疗\",\"payout_reference\":\"account\",\"evidence\":[\"\"]}";
        mvc.perform(post("/api/projects/"+ID+"/disbursements").header("Authorization","Bearer valid")
                .contentType(MediaType.APPLICATION_JSON).content(body)).andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }
    @Test void disabledPaymentsDoNotCreateOrders()throws Exception{
        configure("disabled");
        mvc.perform(post("/api/donations").header("Authorization","Bearer valid").contentType(MediaType.APPLICATION_JSON).content(donation("10.25")))
                .andExpect(status().isServiceUnavailable());verifyNoInteractions(dbop);
    }
    @Test void simulationReturnsFullUpdatedOrderAndHasDeterministicEvent()throws Exception{
        when(dbop.call(eq("finance/order_get"),any(ObjectNode.class))).thenReturn(record("pending"),record("paid"));
        mvc.perform(post("/api/donations/"+ID+"/simulate-payment").header("Authorization","Bearer valid"))
                .andExpect(status().isOk()).andExpect(jsonPath("$.status").value("paid")).andExpect(jsonPath("$.refunds").isArray());
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("finance/complete"),captured.capture());
        assertEquals(ACTOR,captured.getValue().path("actor_id").asText());
        assertEquals("development:payment:"+ID,captured.getValue().path("event_id").asText());
    }
    @Test void simulationCannotBeUsedOutsideExplicitDevelopmentMode()throws Exception{
        configure("disabled");
        mvc.perform(post("/api/donations/"+ID+"/simulate-payment").header("Authorization","Bearer valid"))
                .andExpect(status().isForbidden());
        verify(dbop,never()).call(eq("finance/complete"),any(ObjectNode.class));
    }
    @Test void anonymousFundsOmitActorAndPreservePrivateProjectDenial()throws Exception{
        mvc.perform(get("/api/projects/"+ID+"/funds")).andExpect(status().isOk());
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("finance/funds"),captured.capture());
        assertEquals(ID,captured.getValue().path("project_id").asText());assertFalse(captured.getValue().has("actor_id"));
        verifyNoInteractions(sessions);
        when(dbop.call(eq("finance/funds"),any(ObjectNode.class))).thenThrow(new ResponseStatusException(HttpStatus.FORBIDDEN,"尚未公开"));
        mvc.perform(get("/api/projects/"+ID+"/funds")).andExpect(status().isForbidden());
    }
    @Test void providedInvalidAuthorizationCannotBecomeAnonymousFundsAccess()throws Exception{
        for(String auth:new String[]{"Bearer invalid",""}){
            when(sessions.currentUserId(auth)).thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED,"登录已失效"));
            mvc.perform(get("/api/projects/"+ID+"/funds").header("Authorization",auth)).andExpect(status().isUnauthorized());
        }
        verifyNoInteractions(dbop);
        mvc.perform(get("/api/projects/"+ID+"/funds").header("Authorization","Bearer valid")).andExpect(status().isOk());
        ArgumentCaptor<ObjectNode> captured=ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("finance/funds"),captured.capture());assertEquals(ACTOR,captured.getValue().path("actor_id").asText());
    }
    @Test void missingAuthenticationAndOversizedPaginationFailBeforeDatabase()throws Exception{
        mvc.perform(get("/api/donations")).andExpect(status().isUnauthorized());
        mvc.perform(get("/api/admin/finance/orders?limit=101").header("Authorization","Bearer valid")).andExpect(status().isBadRequest());
        mvc.perform(get("/api/admin/finance/reconciliation?offset=1000001").header("Authorization","Bearer valid")).andExpect(status().isBadRequest());
        verifyNoInteractions(dbop);
    }
    @Test void unsignedOrDisabledCallbacksNeverReachDatabase()throws Exception{
        mvc.perform(post("/api/payments/callback").contentType(MediaType.APPLICATION_JSON).content("{}"))
                .andExpect(status().isForbidden());verifyNoInteractions(dbop);
    }
    @Test void businessConflictIsNotConvertedIntoPaymentSuccess()throws Exception{
        when(dbop.call(eq("finance/order_create"),any(ObjectNode.class))).thenThrow(new ResponseStatusException(HttpStatus.CONFLICT,"幂等键已用于其他金额"));
        mvc.perform(post("/api/donations").header("Authorization","Bearer valid").contentType(MediaType.APPLICATION_JSON).content(donation("10.25")))
                .andExpect(status().isConflict());
    }
}
