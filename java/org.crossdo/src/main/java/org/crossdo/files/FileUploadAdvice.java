package org.crossdo.files;

import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.multipart.MaxUploadSizeExceededException;
import org.springframework.web.multipart.support.MissingServletRequestPartException;

import java.util.LinkedHashMap;
import java.util.Map;

/** Multipart errors can be raised before a controller has been selected. */
@RestControllerAdvice
@Order(Ordered.HIGHEST_PRECEDENCE)
public class FileUploadAdvice {
    @ExceptionHandler(MaxUploadSizeExceededException.class)
    public ResponseEntity<Map<String, Object>> tooLarge(MaxUploadSizeExceededException failure) {
        return error(413, "文件不能超过 10 MiB");
    }

    @ExceptionHandler(MissingServletRequestPartException.class)
    public ResponseEntity<Map<String, Object>> missing(MissingServletRequestPartException failure) {
        return error(400, "请选择要上传的文件");
    }

    private ResponseEntity<Map<String, Object>> error(int status, String message) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("status", status); body.put("message", message);
        return ResponseEntity.status(status).body(body);
    }
}
