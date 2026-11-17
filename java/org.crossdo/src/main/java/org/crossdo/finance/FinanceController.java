package org.crossdo.finance;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import lombok.Getter;
import lombok.Setter;
import org.crossdo.auth.SessionService;
import org.crossdo.infrastructure.DbopClient;
import org.springframework.http.HttpStatus;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;
import javax.validation.Valid;
import javax.validation.constraints.*;
import java.util.ArrayList;
import java.util.List;

@RestController @RequestMapping("/api") @Validated
public class FinanceController {
    static final String UUID_PATTERN="[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}";
    private final DbopClient dbop; private final SessionService sessions;
    private final ObjectMapper mapper; private final PaymentGateway gateway;
    public FinanceController(DbopClient dbop,SessionService sessions,ObjectMapper mapper,PaymentGateway gateway) {
        this.dbop=dbop;this.sessions=sessions;this.mapper=mapper;this.gateway=gateway;
    }
    @GetMapping("/payments/settings")
    public JsonNode settings(){return mapper.createObjectNode().put("mode",gateway.mode());}

    @PostMapping("/donations")
    public JsonNode donate(@RequestHeader(value="Authorization",required=false)String auth,@Valid @RequestBody Donation request) {
        gateway.requireEnabled();ObjectNode body=withActor(auth,request);body.put("mode",gateway.mode());
        return prepareCheckout(dbop.call("finance/order_create",body),body.path("actor_id").asText());
    }
    @PostMapping("/donations/{id}/checkout")
    public JsonNode checkout(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id) {
        ObjectNode body=target(auth,"id",id);
        return prepareCheckout(dbop.call("finance/order_get",body),body.path("actor_id").asText());
    }
    private JsonNode prepareCheckout(JsonNode order,String actor) {
        gateway.requireMatchingMode(order);
        if (!"pending".equals(order.path("status").asText()) || order.hasNonNull("checkout_url") || "development".equals(gateway.mode()))return order;
        JsonNode result=gateway.dispatch("payment",order);
        if (!result.hasNonNull("checkout_url"))throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE,"支付服务暂未返回收银台地址，订单已保留");
        return dbop.call("finance/order_checkout",mapper.createObjectNode().put("actor_id",actor)
                .put("id",order.path("id").asText()).put("checkout_url",result.path("checkout_url").asText()));
    }
    @GetMapping("/donations")
    public JsonNode donations(@RequestHeader(value="Authorization",required=false)String auth,
            @RequestParam(defaultValue="20")@Min(1)@Max(100)int limit,@RequestParam(defaultValue="0")@Min(0)@Max(1000000)int offset) {
        return dbop.call("finance/orders_list",page(auth,limit,offset));
    }
    @GetMapping("/donations/{id}")
    public JsonNode order(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id) {
        return dbop.call("finance/order_get",target(auth,"id",id));
    }
    @PostMapping("/donations/{id}/simulate-payment")
    public JsonNode simulatePayment(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id) {
        ObjectNode body=target(auth,"id",id);JsonNode order=dbop.call("finance/order_get",body);
        dbop.call("finance/complete",gateway.developmentEvent("payment",order,body.path("actor_id").asText()));
        return dbop.call("finance/order_get",body);
    }
    @PostMapping("/donations/{id}/refunds")
    public JsonNode refund(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id,@Valid @RequestBody Refund request) {
        ObjectNode target=target(auth,"id",id);JsonNode order=dbop.call("finance/order_get",target);gateway.requireMatchingMode(order);
        ObjectNode body=mapper.valueToTree(request);body.put("actor_id",target.path("actor_id").asText()).put("id",identifier(id));
        JsonNode refund=dbop.call("finance/refund_create",body);
        if ("pending".equals(refund.path("status").asText()) || "processing".equals(refund.path("status").asText()))gateway.dispatch("refund",refund);
        return refund;
    }
    @GetMapping("/refunds/{id}")
    public JsonNode refundGet(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id){
        return dbop.call("finance/refund_get",target(auth,"id",id));
    }
    @PostMapping("/refunds/{id}/retry")
    public JsonNode retryRefund(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id){
        JsonNode refund=dbop.call("finance/refund_get",target(auth,"id",id));
        if ("pending".equals(refund.path("status").asText()) || "processing".equals(refund.path("status").asText()))gateway.dispatch("refund",refund);
        return refund;
    }
    @PostMapping("/refunds/{id}/simulate-completion")
    public JsonNode simulateRefund(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id) {
        ObjectNode body=target(auth,"id",id);JsonNode refund=dbop.call("finance/refund_get",body);
        dbop.call("finance/complete",gateway.developmentEvent("refund",refund,body.path("actor_id").asText()));
        return dbop.call("finance/refund_get",body);
    }
    @GetMapping("/projects/{id}/funds")
    public JsonNode funds(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id){
        ObjectNode body=mapper.createObjectNode().put("project_id",identifier(id));
        if(auth!=null)body.put("actor_id",sessions.currentUserId(auth));
        return dbop.call("finance/funds",body);
    }
    @PostMapping("/projects/{id}/disbursements")
    public JsonNode disbursement(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id,@Valid @RequestBody Disbursement request){
        gateway.requireEnabled();ObjectNode body=withActor(auth,request);body.put("project_id",identifier(id)).put("mode",gateway.mode());
        return dbop.call("finance/disbursement_create",body);
    }
    @GetMapping("/projects/{id}/disbursements")
    public JsonNode projectDisbursements(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id,
            @RequestParam(defaultValue="20")@Min(1)@Max(100)int limit,@RequestParam(defaultValue="0")@Min(0)@Max(1000000)int offset){
        ObjectNode body=page(auth,limit,offset);body.put("project_id",identifier(id));return dbop.call("finance/disbursements_list",body);
    }
    @GetMapping("/admin/disbursements")
    public JsonNode adminDisbursements(@RequestHeader(value="Authorization",required=false)String auth,
            @RequestParam(defaultValue="20")@Min(1)@Max(100)int limit,@RequestParam(defaultValue="0")@Min(0)@Max(1000000)int offset){
        ObjectNode body=page(auth,limit,offset);body.put("admin",true);return dbop.call("finance/disbursements_list",body);
    }
    @PostMapping("/admin/disbursements/{id}/review")
    public JsonNode reviewDisbursement(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id,@Valid @RequestBody Review request){
        ObjectNode target=target(auth,"id",id);
        if ("approve".equals(request.decision))gateway.requireMatchingMode(dbop.call("finance/disbursement_get",target));
        ObjectNode body=mapper.valueToTree(request);body.put("id",identifier(id)).put("actor_id",target.path("actor_id").asText());
        JsonNode record=dbop.call("finance/disbursement_review",body);
        if ("approve".equals(request.decision)&&"approved".equals(record.path("status").asText()))gateway.dispatch("disbursement",record);
        return record;
    }
    @PostMapping("/disbursements/{id}/simulate-completion")
    public JsonNode simulateDisbursement(@RequestHeader(value="Authorization",required=false)String auth,@PathVariable String id){
        ObjectNode body=target(auth,"id",id);JsonNode record=dbop.call("finance/disbursement_get",body);
        dbop.call("finance/complete",gateway.developmentEvent("disbursement",record,body.path("actor_id").asText()));
        return dbop.call("finance/disbursement_get",body);
    }
    @GetMapping("/admin/finance/orders")
    public JsonNode adminOrders(@RequestHeader(value="Authorization",required=false)String auth,
            @RequestParam(name="project_id",required=false)String project,
            @RequestParam(defaultValue="20")@Min(1)@Max(100)int limit,@RequestParam(defaultValue="0")@Min(0)@Max(1000000)int offset){
        ObjectNode body=page(auth,limit,offset);body.put("admin",true);if(project!=null&&!project.isBlank())body.put("project_id",identifier(project));
        return dbop.call("finance/orders_list",body);
    }
    @GetMapping("/admin/finance/reconciliation")
    public JsonNode reconciliation(@RequestHeader(value="Authorization",required=false)String auth,
            @RequestParam(defaultValue="20")@Min(1)@Max(100)int limit,@RequestParam(defaultValue="0")@Min(0)@Max(1000000)int offset){
        return dbop.call("finance/reconciliation",page(auth,limit,offset));
    }
    @PostMapping("/payments/callback")
    public JsonNode callback(@RequestBody String raw,
            @RequestHeader(value="X-Crossdo-Timestamp",required=false)String timestamp,
            @RequestHeader(value="X-Crossdo-Signature",required=false)String signature){
        return dbop.call("finance/complete",gateway.verifiedCallback(raw,timestamp,signature));
    }
    private ObjectNode actor(String auth){return mapper.createObjectNode().put("actor_id",sessions.currentUserId(auth));}
    private ObjectNode target(String auth,String key,String value){ObjectNode body=actor(auth);body.put(key,identifier(value));return body;}
    private ObjectNode withActor(String auth,Object request){ObjectNode body=mapper.valueToTree(request);body.put("actor_id",sessions.currentUserId(auth));return body;}
    private ObjectNode page(String auth,int limit,int offset){
        if(limit<1||limit>100||offset<0||offset>1000000)
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"分页参数不正确");
        ObjectNode body=actor(auth);body.put("limit",limit).put("offset",offset);return body;
    }
    private static String identifier(String id){if(id==null||!id.matches(UUID_PATTERN))throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"记录编号不正确");return id;}

    @Getter @Setter @JsonIgnoreProperties(ignoreUnknown=true)
    public static class MoneyRequest {
        @NotBlank @Pattern(regexp="(?:0|[1-9][0-9]{0,7})(?:\\.[0-9]{1,2})?") @DecimalMin(value="0",inclusive=false) public String amount;
        @JsonProperty("idempotency_key") @NotNull @Pattern(regexp=UUID_PATTERN) public String idempotencyKey;
    }
    @Getter @Setter public static class Donation extends MoneyRequest {
        @JsonProperty("project_id") @NotNull @Pattern(regexp=UUID_PATTERN) public String projectId;
    }
    @Getter @Setter public static class Refund extends MoneyRequest {
        @NotBlank @Size(max=1000) public String reason;
    }
    @Getter @Setter public static class Disbursement extends MoneyRequest {
        @NotBlank @Size(max=1000) public String purpose;
        @NotNull @Size(max=20) public List<@NotBlank @Size(max=2048)String> evidence=new ArrayList<>();
        @JsonProperty("payout_reference") @NotBlank @Size(max=200) public String payoutReference;
    }
    @Getter @Setter @JsonIgnoreProperties(ignoreUnknown=true) public static class Review {
        @NotNull @Pattern(regexp="approve|reject") public String decision;
        @NotBlank @Size(max=1000) public String note;
    }
}
