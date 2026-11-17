package org.crossdo.web;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.auth.SessionService;
import org.crossdo.infrastructure.DbopClient;
import org.springframework.http.HttpStatus;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

import javax.validation.Valid;
import javax.validation.constraints.Max;
import javax.validation.constraints.Min;

/** Public business endpoints. DBOP remains the only service that reads or writes MySQL. */
@RestController
@RequestMapping("/api")
@Validated
public class WorkflowController {
    private final DbopClient dbop;
    private final SessionService sessions;
    private final ObjectMapper mapper;

    public WorkflowController(DbopClient dbop, SessionService sessions, ObjectMapper mapper) {
        this.dbop = dbop;
        this.sessions = sessions;
        this.mapper = mapper;
    }

    @GetMapping("/me")
    public JsonNode me(@RequestHeader(value = "Authorization", required = false) String authorization) {
        return dbop.call("account/get", actor(authorization));
    }

    @PostMapping("/me/application")
    public JsonNode submitApplication(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @Valid @RequestBody WorkflowRequests.Application request) {
        return dbop.call("application/submit", withActor(request, authorization));
    }

    @GetMapping("/applications")
    public JsonNode applications(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @RequestParam(name = "limit", defaultValue = "20") @Min(1) @Max(100) int limit,
            @RequestParam(name = "offset", defaultValue = "0") @Min(0) int offset) {
        return dbop.call("application/list", page(authorization, limit, offset));
    }

    @GetMapping("/applications/{userId}")
    public JsonNode application(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @PathVariable("userId") String userId) {
        return dbop.call("application/get", target(authorization, "user_id", userId));
    }

    @PostMapping("/applications/{userId}/claim")
    public JsonNode claimApplication(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @PathVariable("userId") String userId) {
        return dbop.call("application/claim", target(authorization, "user_id", userId));
    }

    @PostMapping("/applications/{userId}/review")
    public JsonNode reviewApplication(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @PathVariable("userId") String userId,
            @Valid @RequestBody WorkflowRequests.Review request) {
        ObjectNode body = withActor(request, authorization);
        body.put("user_id", identifier(userId));
        return dbop.call("application/review", body);
    }

    @PostMapping("/projects")
    public JsonNode createProject(@RequestHeader(value = "Authorization", required = false) String authorization) {
        return dbop.call("project/create", actor(authorization));
    }

    @GetMapping("/projects")
    public JsonNode projects(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @RequestParam(name = "limit", defaultValue = "20") @Min(1) @Max(100) int limit,
            @RequestParam(name = "offset", defaultValue = "0") @Min(0) int offset,
            @RequestParam(name = "summary", defaultValue = "false") boolean summary) {
        ObjectNode body = page(authorization, limit, offset);
        if (summary) body.put("summary", true);
        return dbop.call("project/list", body);
    }

    @GetMapping("/projects/{projectId}")
    public JsonNode project(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @PathVariable("projectId") String projectId) {
        return dbop.call("project/get", target(authorization, "project_id", projectId));
    }

    @PostMapping("/projects/{projectId}/submit")
    public JsonNode submitProject(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @PathVariable("projectId") String projectId,
            @Valid @RequestBody WorkflowRequests.ProjectSubmission request) {
        ObjectNode body = withActor(request, authorization);
        body.put("project_id", identifier(projectId));
        return dbop.call("project/submit", body);
    }

    @GetMapping("/tasks/projects")
    public JsonNode projectTasks(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @RequestParam(name = "limit", defaultValue = "20") @Min(1) @Max(100) int limit,
            @RequestParam(name = "offset", defaultValue = "0") @Min(0) int offset) {
        return dbop.call("project/tasks", page(authorization, limit, offset));
    }

    @PostMapping("/projects/{projectId}/claim")
    public JsonNode claimProject(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @PathVariable("projectId") String projectId) {
        return dbop.call("project/claim", target(authorization, "project_id", projectId));
    }

    @PostMapping("/projects/{projectId}/review")
    public JsonNode reviewProject(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @PathVariable("projectId") String projectId,
            @Valid @RequestBody WorkflowRequests.Review request) {
        ObjectNode body = withActor(request, authorization);
        body.put("project_id", identifier(projectId));
        return dbop.call("project/review", body);
    }

    @PostMapping("/projects/{projectId}/pathography")
    public JsonNode appendPathography(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @PathVariable("projectId") String projectId,
            @Valid @RequestBody WorkflowRequests.Pathography request) {
        ObjectNode body = withActor(request, authorization);
        body.put("project_id", identifier(projectId));
        return dbop.call("project/pathography", body);
    }

    private ObjectNode actor(String authorization) {
        return mapper.createObjectNode().put("actor_id", sessions.currentUserId(authorization));
    }

    private ObjectNode withActor(Object request, String authorization) {
        ObjectNode body = mapper.valueToTree(request);
        body.put("actor_id", sessions.currentUserId(authorization));
        return body;
    }

    private ObjectNode target(String authorization, String field, String id) {
        return actor(authorization).put(field, identifier(id));
    }

    private ObjectNode page(String authorization, int limit, int offset) {
        if (limit < 1 || limit > 100 || offset < 0) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "分页参数不正确");
        }
        return actor(authorization).put("limit", limit).put("offset", offset);
    }

    private String identifier(String id) {
        if (id == null || !id.matches("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}")) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "标识格式不正确");
        }
        return id;
    }
}
