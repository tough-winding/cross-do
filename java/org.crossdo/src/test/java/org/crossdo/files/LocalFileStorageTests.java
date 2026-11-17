package org.crossdo.files;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.springframework.http.HttpStatus;
import org.springframework.mock.web.MockMultipartFile;
import org.springframework.web.server.ResponseStatusException;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.stream.Stream;

import static org.junit.jupiter.api.Assertions.*;

class LocalFileStorageTests {
    @TempDir Path directory;
    private static final byte[] PDF = "%PDF-1.7\nsynthetic document\n%%EOF".getBytes(StandardCharsets.UTF_8);

    @Test
    void storesImmutableObjectWithSafeNameAndServerDetectedMime() throws Exception {
        LocalFileStorage storage = new LocalFileStorage(directory.toString());
        LocalFileStorage.StoredFile file = storage.store(new MockMultipartFile(
                "file", "C:\\uploads\\证明\r\n.html", "text/html", PDF));
        assertEquals("证明.pdf", file.name);
        assertEquals("application/pdf", file.mimeType);
        assertEquals(PDF.length, file.size);
        assertArrayEquals(PDF, Files.readAllBytes(storage.existing(file.id, file.size)));
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(PDF);
        StringBuilder hex = new StringBuilder();
        for (byte value : digest) hex.append(String.format("%02x", value & 255));
        assertEquals(hex.toString(), file.sha256);
        try (Stream<Path> files = Files.list(directory)) {
            assertEquals(1, files.count());
        }
    }

    @Test
    void rejectsRenamedHtmlAndCleansIncompleteObjects() throws Exception {
        LocalFileStorage storage = new LocalFileStorage(directory.toString());
        ResponseStatusException error = assertThrows(ResponseStatusException.class, () ->
                storage.store(new MockMultipartFile("file", "proof.pdf", "application/pdf",
                        "<html><script>alert(1)</script></html>".getBytes(StandardCharsets.UTF_8))));
        assertEquals(HttpStatus.BAD_REQUEST, error.getStatus());
        try (Stream<Path> files = Files.list(directory)) {
            assertEquals(0, files.count());
        }
    }

    @Test
    void enforcesStreamingSizeEvenWhenReportedSizeIsFalse() throws Exception {
        LocalFileStorage storage = new LocalFileStorage(directory.toString());
        byte[] tooLarge = new byte[(int)LocalFileStorage.MAX_BYTES + 1];
        System.arraycopy(PDF, 0, tooLarge, 0, PDF.length);
        MockMultipartFile misleading = new MockMultipartFile("file", "proof.pdf", "application/pdf", new byte[] {1}) {
            @Override public InputStream getInputStream() { return new ByteArrayInputStream(tooLarge); }
        };
        ResponseStatusException error = assertThrows(ResponseStatusException.class, () -> storage.store(misleading));
        assertEquals(HttpStatus.BAD_REQUEST, error.getStatus());
        try (Stream<Path> files = Files.list(directory)) {
            assertEquals(0, files.count());
        }
    }

    @Test
    void rejectsPathTraversalSymlinksAndUnexpectedSizes() throws Exception {
        LocalFileStorage storage = new LocalFileStorage(directory.toString());
        assertThrows(ResponseStatusException.class, () -> storage.existing("../outside", 1));
        LocalFileStorage.StoredFile file = storage.store(new MockMultipartFile("file", "proof.pdf", "application/pdf", PDF));
        assertThrows(ResponseStatusException.class, () -> storage.existing(file.id, file.size + 1));
        Path object = storage.existing(file.id, file.size);
        Path target = directory.resolve("external");
        Files.write(target, PDF);
        Files.delete(object);
        Files.createSymbolicLink(object, target);
        assertThrows(ResponseStatusException.class, () -> storage.existing(file.id, file.size));
    }
}
