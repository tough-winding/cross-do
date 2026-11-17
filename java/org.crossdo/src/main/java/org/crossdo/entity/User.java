package org.crossdo.entity;

import java.io.Serializable;
import java.math.BigDecimal;
import java.util.Date;

import com.baomidou.mybatisplus.annotation.TableField;
import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableName;
import com.fasterxml.jackson.annotation.JsonIgnore;
import lombok.Data;
import lombok.ToString;

/** Legacy direct-db mapping; API responses must use explicit public views. */
@TableName("user")
@Data
public class User implements Serializable {
    private static final long serialVersionUID = 1L;

    @TableId
    private String userId;
    private Integer userPermission;
    private String userName;

    @TableField("user_password")
    @JsonIgnore
    @ToString.Exclude
    private String passwd;

    private String avatar;
    private String realName;

    @JsonIgnore
    @ToString.Exclude
    private String idCard;

    private String personalNote;
    private Integer realNameAuthentication;
    private Integer userStatus;
    private Integer existingProjects;
    private String projectId;

    @JsonIgnore
    @ToString.Exclude
    private String weixinOpenid;

    private String phoneNumber;
    private Integer userAge;
    private Date registrationDate;
    private Date loginTime;
    private String currentDisputeId;
    private BigDecimal totalDonationAmount;
}
