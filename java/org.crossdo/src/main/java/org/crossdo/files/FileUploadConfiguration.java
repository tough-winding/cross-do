package org.crossdo.files;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import javax.servlet.MultipartConfigElement;

@Configuration
public class FileUploadConfiguration {
    @Bean
    public MultipartConfigElement multipartConfigElement() {
        return new MultipartConfigElement("", LocalFileStorage.MAX_BYTES,
                LocalFileStorage.MAX_BYTES + 1024L * 1024, 1024 * 1024);
    }
}
