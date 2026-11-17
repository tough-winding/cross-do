package org.crossdo.files;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;
import org.springframework.web.server.ResponseStatusException;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.PosixFilePermissions;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.Normalizer;
import java.util.Locale;
import java.util.UUID;

/** Private, immutable file objects addressed only by server-generated UUIDs. */
@Service
public class LocalFileStorage {
    public static final long MAX_BYTES = 10L * 1024 * 1024;
    private final Path directory;

    public LocalFileStorage(@Value("${crossdo.upload.directory:${CROSSDO_UPLOAD_DIR:./var/uploads}}") String directory) {
        try {
            Path selected = Paths.get(directory).toAbsolutePath().normalize();
            Files.createDirectories(selected);
            this.directory = selected.toRealPath();
            privatePermissions(this.directory, "rwx------");
        } catch (IOException e) {
            throw new IllegalStateException("Private upload directory is unavailable", e);
        }
    }

    public StoredFile store(MultipartFile upload) {
        if (upload == null || upload.isEmpty() || upload.getSize() > MAX_BYTES)
            throw bad("请选择不超过 10 MiB 的非空文件");
        String id = UUID.randomUUID().toString();
        Path temporary = null;
        try {
            temporary = Files.createTempFile(directory, ".upload-", ".tmp");
            privatePermissions(temporary, "rw-------");
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            long count = 0;
            byte[] prefix = new byte[16];
            int prefixSize = 0;
            try (InputStream input = upload.getInputStream(); OutputStream output = Files.newOutputStream(temporary)) {
                byte[] buffer = new byte[8192];
                for (int size; (size = input.read(buffer)) != -1; ) {
                    if (size == 0) continue;
                    count += size;
                    if (count > MAX_BYTES) throw bad("文件不能超过 10 MiB");
                    if (prefixSize < prefix.length) {
                        int copy = Math.min(size, prefix.length - prefixSize);
                        System.arraycopy(buffer, 0, prefix, prefixSize, copy);
                        prefixSize += copy;
                    }
                    digest.update(buffer, 0, size);
                    output.write(buffer, 0, size);
                }
            }
            String mime = detectType(prefix, prefixSize);
            String name = safeName(upload.getOriginalFilename(), mime);
            Path destination = resolve(id);
            Files.move(temporary, destination, StandardCopyOption.ATOMIC_MOVE);
            temporary = null;
            return new StoredFile(id, name, count, mime, hex(digest.digest()));
        } catch (IOException e) {
            throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "文件存储暂时不可用");
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException(e);
        } finally {
            if (temporary != null) {
                try { Files.deleteIfExists(temporary); } catch (IOException ignored) { /* Incomplete temp file is never served. */ }
            }
        }
    }

    public Path existing(String id, long expectedSize) {
        Path file = resolve(id);
        try {
            if (!Files.isRegularFile(file, LinkOption.NOFOLLOW_LINKS) || Files.size(file) != expectedSize)
                throw new ResponseStatusException(HttpStatus.NOT_FOUND, "文件暂不可用");
            return file;
        } catch (IOException e) {
            throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "文件存储暂时不可用");
        }
    }

    /** Only for a definite metadata rejection. Unknown commit outcomes retain bytes. */
    public void removeRejected(String id) {
        try { Files.deleteIfExists(resolve(id)); }
        catch (IOException ignored) { /* Orphan objects have no database access grant. */ }
    }

    private Path resolve(String id) {
        String canonical;
        try { canonical = UUID.fromString(id).toString(); }
        catch (IllegalArgumentException | NullPointerException e) { throw bad("文件编号不正确"); }
        if (!canonical.equalsIgnoreCase(id)) throw bad("文件编号不正确");
        Path path = directory.resolve(canonical + ".bin").normalize();
        if (!path.getParent().equals(directory)) throw bad("文件编号不正确");
        return path;
    }

    private static String detectType(byte[] prefix, int size) {
        if (size >= 8 && (prefix[0] & 255) == 137 && prefix[1] == 'P' && prefix[2] == 'N' &&
                prefix[3] == 'G' && prefix[4] == 13 && prefix[5] == 10 && prefix[6] == 26 && prefix[7] == 10)
            return "image/png";
        if (size >= 3 && (prefix[0] & 255) == 255 && (prefix[1] & 255) == 216 && (prefix[2] & 255) == 255)
            return "image/jpeg";
        if (size >= 8 && prefix[0] == '%' && prefix[1] == 'P' && prefix[2] == 'D' && prefix[3] == 'F' &&
                prefix[4] == '-' && (prefix[5] == '1' || prefix[5] == '2') && prefix[6] == '.' &&
                prefix[7] >= '0' && prefix[7] <= '9')
            return "application/pdf";
        throw bad("仅支持 PNG、JPEG 图片和 PDF 文件");
    }

    static String safeName(String original, String mime) {
        String name = Normalizer.normalize(original == null ? "材料" : original, Normalizer.Form.NFC)
                .replace('\\', '/');
        name = name.substring(name.lastIndexOf('/') + 1).replaceAll("[\\p{Cntrl}\\p{Cf}]", "").trim();
        int dot = name.lastIndexOf('.');
        if (dot > 0) name = name.substring(0, dot);
        if (name.isEmpty() || name.equals(".") || name.equals("..")) name = "材料";
        int max = Math.min(name.codePointCount(0, name.length()), 180);
        name = name.substring(0, name.offsetByCodePoints(0, max));
        String extension = mime.equals("application/pdf") ? ".pdf" : mime.equals("image/png") ? ".png" : ".jpg";
        return name + extension;
    }

    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) result.append(String.format(Locale.ROOT, "%02x", value & 255));
        return result.toString();
    }

    private static void privatePermissions(Path path, String permissions) throws IOException {
        try { Files.setPosixFilePermissions(path, PosixFilePermissions.fromString(permissions)); }
        catch (UnsupportedOperationException ignored) { /* Non-POSIX hosts use directory ACLs. */ }
    }

    private static ResponseStatusException bad(String message) {
        return new ResponseStatusException(HttpStatus.BAD_REQUEST, message);
    }

    public static final class StoredFile {
        public final String id;
        public final String name;
        public final long size;
        public final String mimeType;
        public final String sha256;

        StoredFile(String id, String name, long size, String mimeType, String sha256) {
            this.id = id; this.name = name; this.size = size; this.mimeType = mimeType; this.sha256 = sha256;
        }
    }
}
