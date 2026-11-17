package org.crossdo.files;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.crossdo.auth.SessionService;
import org.crossdo.infrastructure.DbopClient;
import org.crossdo.web.ApiExceptionHandler;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.mockito.ArgumentCaptor;
import org.springframework.http.HttpStatus;
import org.springframework.mock.web.MockMultipartFile;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import org.springframework.web.server.ResponseStatusException;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.*;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

class FileControllerTests {
    private static final String USER = "11111111-1111-4111-8111-111111111111";
    private static final byte[] PDF = "%PDF-1.7\nsynthetic document\n%%EOF".getBytes(StandardCharsets.UTF_8);
    @TempDir Path directory;
    private DbopClient dbop;
    private SessionService sessions;
    private LocalFileStorage storage;
    private ObjectMapper mapper;
    private MockMvc mvc;

    @BeforeEach
    void setup() {
        dbop = mock(DbopClient.class); sessions = mock(SessionService.class);
        mapper = new ObjectMapper(); storage = new LocalFileStorage(directory.toString());
        when(sessions.currentUserId("Bearer valid")).thenReturn(USER);
        when(sessions.currentUserId(isNull())).thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED, "请先登录"));
        mvc = MockMvcBuilders.standaloneSetup(new FileController(dbop, sessions, mapper, storage))
                .setControllerAdvice(new FileUploadAdvice(), new ApiExceptionHandler()).build();
    }

    private MockMultipartFile document() {
        return new MockMultipartFile("file", "proof.pdf", "application/pdf", PDF);
    }

    private long storedObjects() throws Exception {
        try (Stream<Path> files = Files.list(directory)) { return files.count(); }
    }

    @Test
    void uploadBindsActorFromSessionAndReturnsOpaqueReference() throws Exception {
        when(dbop.call(eq("files/register"), any(ObjectNode.class))).thenAnswer(call -> {
            ObjectNode request = call.getArgument(1);
            return mapper.createObjectNode().put("id", request.path("id").asText())
                    .put("url", "/api/files/" + request.path("id").asText())
                    .put("name", "proof.pdf").put("size", PDF.length).put("mime_type", "application/pdf");
        });
        mvc.perform(multipart("/api/files").file(document()).param("purpose", "case")
                        .param("actor_id", "attacker").header("Authorization", "Bearer valid"))
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.url").value(org.hamcrest.Matchers.startsWith("/api/files/")))
                .andExpect(jsonPath("$.storage_key").doesNotExist())
                .andExpect(jsonPath("$.sha256").doesNotExist());
        ArgumentCaptor<ObjectNode> body = ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("files/register"), body.capture());
        assertEquals(USER, body.getValue().path("actor_id").asText());
        assertEquals("case", body.getValue().path("purpose").asText());
        assertEquals(64, body.getValue().path("sha256").asText().length());
        assertEquals(1, storedObjects());
    }

    @Test
    void authenticationAndPurposeChecksHappenBeforeStorage() throws Exception {
        mvc.perform(multipart("/api/files").file(document()).param("purpose", "application"))
                .andExpect(status().isUnauthorized());
        mvc.perform(multipart("/api/files").file(document()).param("purpose", "project")
                        .param("project_id", "../invalid").header("Authorization", "Bearer valid"))
                .andExpect(status().isBadRequest());
        assertEquals(0, storedObjects());
        verifyNoInteractions(dbop);
    }

    @Test
    void definiteMetadataRejectionRemovesObjectButUnknownCommitKeepsIt() throws Exception {
        when(dbop.call(eq("files/register"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.FORBIDDEN, "not owner"));
        mvc.perform(multipart("/api/files").file(document()).param("purpose", "application")
                        .header("Authorization", "Bearer valid")).andExpect(status().isForbidden());
        assertEquals(0, storedObjects());
        when(dbop.call(eq("files/register"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "unknown commit"));
        mvc.perform(multipart("/api/files").file(document()).param("purpose", "application")
                        .header("Authorization", "Bearer valid")).andExpect(status().isServiceUnavailable());
        assertEquals(1, storedObjects());
    }

    @Test
    void publicDownloadStillConsultsDbAclAndUsesAttachmentHeaders() throws Exception {
        LocalFileStorage.StoredFile file = storage.store(document());
        when(dbop.call(eq("files/access"), any(ObjectNode.class))).thenReturn(mapper.createObjectNode()
                .put("id", file.id).put("name", file.name).put("size", file.size).put("mime_type", file.mimeType));
        mvc.perform(get("/api/files/" + file.id)).andExpect(status().isOk())
                .andExpect(header().string("Cache-Control", "no-store"))
                .andExpect(header().string("X-Content-Type-Options", "nosniff"))
                .andExpect(header().string("Content-Disposition", org.hamcrest.Matchers.startsWith("attachment;")))
                .andExpect(content().bytes(PDF));
        ArgumentCaptor<ObjectNode> body = ArgumentCaptor.forClass(ObjectNode.class);
        verify(dbop).call(eq("files/access"), body.capture());
        assertFalse(body.getValue().has("actor_id"));
    }

    @Test
    void privateDownloadDenialAndInvalidBearerCannotFallBackToAnonymous() throws Exception {
        String id = "22222222-2222-4222-8222-222222222222";
        when(dbop.call(eq("files/access"), any(ObjectNode.class)))
                .thenThrow(new ResponseStatusException(HttpStatus.FORBIDDEN, "This material is private"));
        mvc.perform(get("/api/files/" + id)).andExpect(status().isForbidden());
        reset(dbop);
        when(sessions.currentUserId("Bearer invalid"))
                .thenThrow(new ResponseStatusException(HttpStatus.UNAUTHORIZED, "expired"));
        mvc.perform(get("/api/files/" + id).header("Authorization", "Bearer invalid"))
                .andExpect(status().isUnauthorized());
        verifyNoInteractions(dbop);
    }
}
