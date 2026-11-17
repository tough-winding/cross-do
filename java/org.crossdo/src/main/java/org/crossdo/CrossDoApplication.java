package org.crossdo;

import org.mybatis.spring.annotation.MapperScan;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Profile;

@SpringBootApplication
public class CrossDoApplication {

    public static void main(String[] args) {
        SpringApplication.run(CrossDoApplication.class, args);
    }

    /** Direct SQL access is opt-in; normal requests use the C DBOP service. */
    @Configuration
    @Profile("direct-db")
    @MapperScan("org.crossdo.mapper")
    static class DirectDatabaseConfiguration {
    }
}
