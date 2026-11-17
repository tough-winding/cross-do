package org.crossdo.operations;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.auth.SessionService;
import org.crossdo.infrastructure.DbopClient;
import org.springframework.http.HttpStatus;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;
import lombok.Getter;
import lombok.Setter;

import javax.validation.Valid;
import javax.validation.constraints.*;
import java.util.ArrayList;
import java.util.List;

/** Operational actions retain database-side authorization and transactional audits. */
@RestController
@RequestMapping("/api")
@Validated
public class OperationsController {
    private final DbopClient dbop;
    private final SessionService sessions;
    private final ObjectMapper mapper;
    public OperationsController(DbopClient dbop, SessionService sessions, ObjectMapper mapper) {
        this.dbop = dbop; this.sessions = sessions; this.mapper = mapper;
    }

    @GetMapping("/admin/overview")
    public JsonNode overview(@RequestHeader(value="Authorization",required=false) String auth) {
        return dbop.call("ops/overview", actor(auth));
    }

    @GetMapping("/admin/users")
    public JsonNode users(@RequestHeader(value="Authorization",required=false) String auth,
            @RequestParam(defaultValue="0") int role, @RequestParam(defaultValue="") @Size(max=16) String state,
            @RequestParam(defaultValue="") @Size(max=64) String q,
            @RequestParam(defaultValue="20") @Min(1) @Max(100) int limit,
            @RequestParam(defaultValue="0") @Min(0) @Max(1000000) int offset) {
        return dbop.call("ops/users", page(auth,limit,offset).put("role",role).put("state",state).put("q",q));
    }

    @PostMapping("/admin/users/{id}/certification")
    public JsonNode certify(@RequestHeader(value="Authorization",required=false) String auth,
            @PathVariable String id,@Valid @RequestBody Certification request) {
        return dbop.call("ops/certification", body(auth,request).put("user_id",uuid(id)));
    }

    @PostMapping("/admin/users/{id}/state")
    public JsonNode accountState(@RequestHeader(value="Authorization",required=false) String auth,
            @PathVariable String id,@Valid @RequestBody AccountState request) {
        return dbop.call("ops/account-state",body(auth,request).put("user_id",uuid(id)));
    }

    @GetMapping("/admin/tasks")
    public JsonNode tasks(@RequestHeader(value="Authorization",required=false) String auth,
            @RequestParam @Pattern(regexp="application|project") String kind,
            @RequestParam(defaultValue="20") @Min(1) @Max(100) int limit,
            @RequestParam(defaultValue="0") @Min(0) @Max(1000000) int offset) {
        return dbop.call("ops/tasks",page(auth,limit,offset).put("kind",kind));
    }

    @PostMapping("/admin/tasks/{kind}/{id}/reassign")
    public JsonNode reassign(@RequestHeader(value="Authorization",required=false) String auth,
            @PathVariable @Pattern(regexp="application|project") String kind,
            @PathVariable String id,@Valid @RequestBody Reassignment request) {
        if (request.volunteerId!=null) uuid(request.volunteerId);
        return dbop.call("ops/reassign",body(auth,request).put("kind",kind).put("id",uuid(id)));
    }

    @GetMapping("/notifications")
    public JsonNode notifications(@RequestHeader(value="Authorization",required=false) String auth,
            @RequestParam(defaultValue="20") @Min(1) @Max(100) int limit,
            @RequestParam(defaultValue="0") @Min(0) @Max(1000000) int offset) {
        return dbop.call("ops/notifications",page(auth,limit,offset));
    }

    @GetMapping("/notifications/unread-count")
    public JsonNode unread(@RequestHeader(value="Authorization",required=false) String auth) {
        return dbop.call("ops/notifications-unread",actor(auth));
    }

    @PostMapping("/notifications/{id}/read")
    public JsonNode read(@RequestHeader(value="Authorization",required=false) String auth,@PathVariable String id) {
        return dbop.call("ops/notification-read",actor(auth).put("id",uuid(id)));
    }

    @PostMapping("/notifications/read-all")
    public JsonNode readAll(@RequestHeader(value="Authorization",required=false) String auth) {
        return dbop.call("ops/notifications-read-all",actor(auth));
    }

    @PostMapping("/cases")
    public JsonNode createCase(@RequestHeader(value="Authorization",required=false) String auth,
            @Valid @RequestBody CaseCreation request) {
        if(request.projectId!=null) uuid(request.projectId);
        if(request.targetUserId!=null) uuid(request.targetUserId);
        if(request.parentId!=null) uuid(request.parentId);
        if("report".equals(request.kind) && ((request.projectId==null)==(request.targetUserId==null)))
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"举报需要指定一个项目或账号");
        if("appeal".equals(request.kind) && request.parentId==null)
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"申诉需要关联已处理的举报");
        return dbop.call("ops/case-create",body(auth,request));
    }

    @GetMapping("/cases")
    public JsonNode cases(@RequestHeader(value="Authorization",required=false) String auth,
            @RequestParam(defaultValue="20") @Min(1) @Max(100) int limit,
            @RequestParam(defaultValue="0") @Min(0) @Max(1000000) int offset) {
        return dbop.call("ops/cases",page(auth,limit,offset));
    }

    @GetMapping("/cases/{id}")
    public JsonNode caseDetail(@RequestHeader(value="Authorization",required=false) String auth,@PathVariable String id) {
        return dbop.call("ops/case-get",actor(auth).put("id",uuid(id)));
    }

    @GetMapping("/admin/cases")
    public JsonNode adminCases(@RequestHeader(value="Authorization",required=false) String auth,
            @RequestParam(defaultValue="") @Pattern(regexp="|open|resolved|dismissed") String status,
            @RequestParam(defaultValue="20") @Min(1) @Max(100) int limit,
            @RequestParam(defaultValue="0") @Min(0) @Max(1000000) int offset) {
        return dbop.call("ops/admin-cases",page(auth,limit,offset).put("status",status));
    }

    @PostMapping("/admin/cases/{id}/resolve")
    public JsonNode resolveCase(@RequestHeader(value="Authorization",required=false) String auth,
            @PathVariable String id,@Valid @RequestBody CaseDecision request) {
        return dbop.call("ops/case-resolve",body(auth,request).put("id",uuid(id)));
    }

    @PostMapping("/projects/{id}/closure")
    public JsonNode requestClosure(@RequestHeader(value="Authorization",required=false) String auth,
            @PathVariable String id,@Valid @RequestBody ClosureRequest request) {
        return dbop.call("ops/closure-request",body(auth,request).put("project_id",uuid(id)));
    }

    @GetMapping("/projects/{id}/closure")
    public JsonNode closure(@RequestHeader(value="Authorization",required=false) String auth,@PathVariable String id) {
        ObjectNode request=auth==null?mapper.createObjectNode():actor(auth);
        return dbop.call("ops/closure-get",request.put("project_id",uuid(id)));
    }

    @GetMapping("/admin/closures")
    public JsonNode closures(@RequestHeader(value="Authorization",required=false) String auth,
            @RequestParam(defaultValue="20") @Min(1) @Max(100) int limit,
            @RequestParam(defaultValue="0") @Min(0) @Max(1000000) int offset) {
        return dbop.call("ops/closures",page(auth,limit,offset));
    }

    @PostMapping("/admin/projects/{id}/closure/review")
    public JsonNode reviewClosure(@RequestHeader(value="Authorization",required=false) String auth,
            @PathVariable String id,@Valid @RequestBody ClosureDecision request) {
        return dbop.call("ops/closure-review",body(auth,request).put("project_id",uuid(id)));
    }

    private ObjectNode actor(String auth) {
        return mapper.createObjectNode().put("actor_id",sessions.currentUserId(auth));
    }
    private ObjectNode body(String auth,Object request) {
        ObjectNode result=mapper.valueToTree(request);
        result.put("actor_id",sessions.currentUserId(auth)); return result;
    }
    private ObjectNode page(String auth,int limit,int offset) {
        if(limit<1||limit>100||offset<0||offset>1000000)
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"分页参数不正确");
        return actor(auth).put("limit",limit).put("offset",offset);
    }
    private String uuid(String value) {
        if(value==null||!value.matches("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}"))
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST,"标识格式不正确");
        return value;
    }

    @JsonIgnoreProperties(ignoreUnknown=true)
    public static class Note { @NotBlank @Size(max=1300) public String note; }
    public static class Certification extends Note { @NotNull public Boolean approved; }
    public static class AccountState extends Note { @NotNull @Pattern(regexp="active|frozen") public String state; }
    public static class Reassignment extends Note { @JsonProperty("volunteer_id") public String volunteerId; }
    @JsonIgnoreProperties(ignoreUnknown=true)
    @Getter @Setter
    public static class CaseCreation {
        @NotNull @Pattern(regexp="report|appeal") public String kind;
        @JsonProperty("project_id") public String projectId;
        @JsonProperty("target_user_id") public String targetUserId;
        @JsonProperty("parent_id") public String parentId;
        @NotBlank @Size(max=2000) public String reason;
        @NotNull @Size(max=20) public List<@NotBlank @Size(max=2048) @Pattern(regexp="https?://.+|/api/files/[0-9a-fA-F-]{36}") String> evidence=new ArrayList<>();
    }
    public static class CaseDecision extends Note { @NotNull @Pattern(regexp="resolved|dismissed") public String decision; }
    @JsonIgnoreProperties(ignoreUnknown=true)
    public static class ClosureRequest {
        @NotBlank @Size(max=2000) public String reason;
        @JsonProperty("remaining_plan") @NotNull @Pattern(regexp="refund|fully_used") public String remainingPlan;
    }
    public static class ClosureDecision extends Note { @NotNull @Pattern(regexp="approve|reject") public String decision; }
}
