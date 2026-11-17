package org.crossdo.files;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.auth.SessionService;
import org.crossdo.infrastructure.DbopClient;
import org.springframework.core.io.FileSystemResource;
import org.springframework.core.io.Resource;
import org.springframework.http.ContentDisposition;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;
import org.springframework.web.server.ResponseStatusException;

import java.nio.charset.StandardCharsets;
import java.util.UUID;

@RestController
@RequestMapping("/api/files")
public class FileController {
    private final DbopClient dbop;
    private final SessionService sessions;
    private final ObjectMapper mapper;
    private final LocalFileStorage storage;

    public FileController(DbopClient dbop, SessionService sessions, ObjectMapper mapper, LocalFileStorage storage) {
        this.dbop = dbop; this.sessions = sessions; this.mapper = mapper; this.storage = storage;
    }

    @PostMapping(consumes = MediaType.MULTIPART_FORM_DATA_VALUE)
    public ResponseEntity<JsonNode> upload(
            @RequestHeader(value = "Authorization", required = false) String authorization,
            @RequestParam("file") MultipartFile file, @RequestParam("purpose") String purpose,
            @RequestParam(value = "project_id", required = false) String projectId) {
        String actor = sessions.currentUserId(authorization);
        if (!"application".equals(purpose) && !"project".equals(purpose) && !"case".equals(purpose))
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "请指定材料用途");
        if ("project".equals(purpose)) projectId = identifier(projectId);
        else if (projectId != null)
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "此类材料不能直接绑定项目");
        LocalFileStorage.StoredFile stored = storage.store(file);
        ObjectNode body = mapper.createObjectNode().put("actor_id", actor).put("id", stored.id)
                .put("purpose", purpose).put("name", stored.name).put("size", stored.size)
                .put("mime_type", stored.mimeType).put("sha256", stored.sha256);
        if (projectId != null) body.put("project_id", projectId);
        try {
            return ResponseEntity.status(HttpStatus.CREATED).body(dbop.call("files/register", body));
        } catch (ResponseStatusException failure) {
            // A timeout/5xx can follow a committed metadata insert. Keeping its
            // private bytes is safer than making that committed record unreadable.
            if (failure.getStatus().is4xxClientError()) storage.removeRejected(stored.id);
            throw failure;
        }
    }

    @GetMapping("/{id}")
    public ResponseEntity<Resource> download(@PathVariable("id") String id,
            @RequestHeader(value = "Authorization", required = false) String authorization) {
        String canonical = identifier(id);
        ObjectNode body = mapper.createObjectNode().put("id", canonical);
        // A caller supplying a bad token does not fall back to anonymous access.
        if (authorization != null) body.put("actor_id", sessions.currentUserId(authorization));
        JsonNode metadata = dbop.call("files/access", body);
        String mime = metadata.path("mime_type").asText();
        if (!mime.equals("image/png") && !mime.equals("image/jpeg") && !mime.equals("application/pdf"))
            throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "文件元数据不可用");
        long size = metadata.path("size").asLong(-1);
        if (size < 1 || size > LocalFileStorage.MAX_BYTES)
            throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "文件元数据不可用");
        Resource resource = new FileSystemResource(storage.existing(canonical, size));
        return ResponseEntity.ok()
                .header(HttpHeaders.CONTENT_DISPOSITION, ContentDisposition.attachment()
                        .filename(metadata.path("name").asText("材料"), StandardCharsets.UTF_8).build().toString())
                .header(HttpHeaders.CACHE_CONTROL, "no-store")
                .header("X-Content-Type-Options", "nosniff")
                .contentType(MediaType.parseMediaType(mime))
                .contentLength(size)
                .body(resource);
    }

    private String identifier(String value) {
        try {
            String canonical = UUID.fromString(value).toString();
            if (!canonical.equalsIgnoreCase(value)) throw new IllegalArgumentException();
            return canonical;
        } catch (IllegalArgumentException | NullPointerException failure) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "文件或项目编号不正确");
        }
    }
}
