package org.crossdo.web;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Getter;
import lombok.Setter;

import javax.validation.Valid;
import javax.validation.constraints.*;
import java.util.ArrayList;
import java.util.List;

/** Only application data may come from the caller; identity and state come from the server. */
public final class WorkflowRequests {
    private WorkflowRequests() { }

    @JsonIgnoreProperties(ignoreUnknown = true)
    @Getter @Setter
    public static class Application {
        @JsonProperty("real_name")
        @NotBlank @Size(max = 20)
        public String realName;

        @JsonProperty("id_card")
        @NotNull @Pattern(regexp = "[0-9]{17}[0-9Xx]")
        public String idCard;

        @JsonProperty("user_age")
        @NotNull @Min(0) @Max(120)
        public Integer userAge;

        @NotNull @Size(min = 1, max = 20)
        public List<@NotBlank @Size(max = 2048) String> materials;
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    @Getter @Setter
    public static class Review {
        @NotNull @Pattern(regexp = "approve|correction|unreachable")
        public String decision;

        @NotBlank @Size(max = 2000)
        public String note;
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    @Getter @Setter
    public static class Entry {
        @NotBlank @Size(max = 10000)
        public String content;

        @NotNull @Size(max = 20)
        public List<@NotBlank @Size(max = 2048) String> attachments = new ArrayList<>();
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    @Getter @Setter
    public static class ProjectSubmission {
        @JsonProperty("donation_pool_limit")
        @NotNull @Pattern(regexp = "(?:0|[1-9][0-9]{0,7})(?:\\.[0-9]{1,2})?")
        @DecimalMin(value = "0", inclusive = false)
        public String donationPoolLimit;

        @NotNull @Size(min = 1, max = 100)
        public List<@NotNull @Valid Entry> pathography;
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    @Getter @Setter
    public static class Pathography {
        @NotNull @Valid
        public Entry entry;
    }
}
