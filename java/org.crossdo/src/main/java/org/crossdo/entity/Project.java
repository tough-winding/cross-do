package org.crossdo.entity;

import java.io.Serializable;
import java.math.BigDecimal;
import java.util.Date;

import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableName;
import lombok.Data;

/** Monetary values map exactly to the schema's DECIMAL(10,2) columns. */
@TableName("project")
@Data
public class Project implements Serializable {
    private static final long serialVersionUID = 1L;

    @TableId
    private String projectId;
    private String userId;
    private String userName;
    private Integer userAge;
    private String realName;
    private Integer projectStatus;
    private String verificationRecord;
    private String currentVolunteer;
    private String previousVolunteers;
    private Integer questionIsVolunteerAssigned;
    private String currentDisputeId;
    private Date projectCreateTime;
    private BigDecimal donationPoolLimit;
    private BigDecimal currentDonationsReceived;
    private BigDecimal currentUsedDonations;
    private Date lastUpdateTime;
    private String pathography;
}
