package org.crossdo.entity;

import java.io.Serializable;
import java.util.Date;

import com.baomidou.mybatisplus.annotations.TableField;
import com.baomidou.mybatisplus.annotations.TableId;
import com.baomidou.mybatisplus.annotations.TableName;

import lombok.Data;

/**
 * @TableName user
 */
@TableName(value = "user")
@Data
public class User implements Serializable {
    /**
     *
     */
    @TableId
    private String userId;

    /**
     *
     */
    private Integer userPermission;

    /**
     *
     */
    private String userName;

    /**
     *
     */
    private String passwd;

    /**
     *
     */
    private String avatar;

    /**
     *
     */
    private String realName;

    /**
     *
     */
    private String idCard;

    /**
     *
     */
    private String personalNote;

    /**
     *
     */
    private Integer realNameAuthentication;

    /**
     *
     */
    private Integer userStatus;

    /**
     *
     */
    private Integer existingProjects;

    /**
     *
     */
    private String projectId;

    /**
     *
     */
    private String weixinOpenid;

    /**
     *
     */
    private String phoneNumber;

    /**
     *
     */
    private Integer userAge;

    /**
     *
     */
    private Date registrationDate;

    /**
     *
     */
    private Date loginTime;

    /**
     *
     */
    private String currentDisputeId;

    @TableField(exist = false)
    private static final long serialVersionUID = 1L;

    @Override
    public boolean equals(Object that) {
        if (this == that) {
            return true;
        }
        if (that == null) {
            return false;
        }
        if (getClass() != that.getClass()) {
            return false;
        }
        User other = (User) that;
        return (this.getUserId() == null ? other.getUserId() == null : this.getUserId().equals(other.getUserId()))
                && (this.getUserPermission() == null ? other.getUserPermission() == null : this.getUserPermission().equals(other.getUserPermission()))
                && (this.getUserName() == null ? other.getUserName() == null : this.getUserName().equals(other.getUserName()))
                && (this.getPasswd() == null ? other.getPasswd() == null : this.getPasswd().equals(other.getPasswd()))
                && (this.getAvatar() == null ? other.getAvatar() == null : this.getAvatar().equals(other.getAvatar()))
                && (this.getRealName() == null ? other.getRealName() == null : this.getRealName().equals(other.getRealName()))
                && (this.getIdCard() == null ? other.getIdCard() == null : this.getIdCard().equals(other.getIdCard()))
                && (this.getPersonalNote() == null ? other.getPersonalNote() == null : this.getPersonalNote().equals(other.getPersonalNote()))
                && (this.getRealNameAuthentication() == null ? other.getRealNameAuthentication() == null
                                                             : this.getRealNameAuthentication().equals(other.getRealNameAuthentication()))
                && (this.getUserStatus() == null ? other.getUserStatus() == null : this.getUserStatus().equals(other.getUserStatus()))
                && (this.getExistingProjects() == null ? other.getExistingProjects() == null
                                                       : this.getExistingProjects().equals(other.getExistingProjects()))
                && (this.getProjectId() == null ? other.getProjectId() == null : this.getProjectId().equals(other.getProjectId()))
                && (this.getWeixinOpenid() == null ? other.getWeixinOpenid() == null : this.getWeixinOpenid().equals(other.getWeixinOpenid()))
                && (this.getPhoneNumber() == null ? other.getPhoneNumber() == null : this.getPhoneNumber().equals(other.getPhoneNumber()))
                && (this.getUserAge() == null ? other.getUserAge() == null : this.getUserAge().equals(other.getUserAge()))
                && (this.getRegistrationDate() == null ? other.getRegistrationDate() == null
                                                       : this.getRegistrationDate().equals(other.getRegistrationDate()))
                && (this.getLoginTime() == null ? other.getLoginTime() == null : this.getLoginTime().equals(other.getLoginTime()))
                && (this.getCurrentDisputeId() == null ? other.getCurrentDisputeId() == null
                                                       : this.getCurrentDisputeId().equals(other.getCurrentDisputeId()));
    }

    @Override
    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((getUserId() == null) ? 0 : getUserId().hashCode());
        result = prime * result + ((getUserPermission() == null) ? 0 : getUserPermission().hashCode());
        result = prime * result + ((getUserName() == null) ? 0 : getUserName().hashCode());
        result = prime * result + ((getPasswd() == null) ? 0 : getPasswd().hashCode());
        result = prime * result + ((getAvatar() == null) ? 0 : getAvatar().hashCode());
        result = prime * result + ((getRealName() == null) ? 0 : getRealName().hashCode());
        result = prime * result + ((getIdCard() == null) ? 0 : getIdCard().hashCode());
        result = prime * result + ((getPersonalNote() == null) ? 0 : getPersonalNote().hashCode());
        result = prime * result + ((getRealNameAuthentication() == null) ? 0 : getRealNameAuthentication().hashCode());
        result = prime * result + ((getUserStatus() == null) ? 0 : getUserStatus().hashCode());
        result = prime * result + ((getExistingProjects() == null) ? 0 : getExistingProjects().hashCode());
        result = prime * result + ((getProjectId() == null) ? 0 : getProjectId().hashCode());
        result = prime * result + ((getWeixinOpenid() == null) ? 0 : getWeixinOpenid().hashCode());
        result = prime * result + ((getPhoneNumber() == null) ? 0 : getPhoneNumber().hashCode());
        result = prime * result + ((getUserAge() == null) ? 0 : getUserAge().hashCode());
        result = prime * result + ((getRegistrationDate() == null) ? 0 : getRegistrationDate().hashCode());
        result = prime * result + ((getLoginTime() == null) ? 0 : getLoginTime().hashCode());
        result = prime * result + ((getCurrentDisputeId() == null) ? 0 : getCurrentDisputeId().hashCode());
        return result;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(getClass().getSimpleName());
        sb.append(" [");
        sb.append("Hash = ").append(hashCode());
        sb.append(", userId=").append(userId);
        sb.append(", userPermission=").append(userPermission);
        sb.append(", userName=").append(userName);
        sb.append(", passwd=").append(passwd);
        sb.append(", avatar=").append(avatar);
        sb.append(", realName=").append(realName);
        sb.append(", idCard=").append(idCard);
        sb.append(", personalNote=").append(personalNote);
        sb.append(", realNameAuthentication=").append(realNameAuthentication);
        sb.append(", userStatus=").append(userStatus);
        sb.append(", existingProjects=").append(existingProjects);
        sb.append(", projectId=").append(projectId);
        sb.append(", weixinOpenid=").append(weixinOpenid);
        sb.append(", phoneNumber=").append(phoneNumber);
        sb.append(", userAge=").append(userAge);
        sb.append(", registrationDate=").append(registrationDate);
        sb.append(", loginTime=").append(loginTime);
        sb.append(", currentDisputeId=").append(currentDisputeId);
        sb.append(", serialVersionUID=").append(serialVersionUID);
        sb.append("]");
        return sb.toString();
    }
}