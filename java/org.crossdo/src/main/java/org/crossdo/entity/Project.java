package org.crossdo.entity;

import java.io.Serializable;
import java.util.Date;

import com.baomidou.mybatisplus.annotations.TableField;
import com.baomidou.mybatisplus.annotations.TableId;
import com.baomidou.mybatisplus.annotations.TableName;

import lombok.Data;

/**
 * @TableName project
 */
@TableName(value = "project")
@Data
public class Project implements Serializable {
    /**
     *
     */
    @TableId
    private String projectId;

    /**
     *
     */
    private String userName;

    /**
     *
     */
    private Integer userAge;

    /**
     *
     */
    private String realName;

    /**
     *
     */
    private Integer projectStatus;

    /**
     *
     */
    private String verificationRecord;

    /**
     *
     */
    private String currentVolunteer;

    /**
     *
     */
    private String previousVolunteers;

    /**
     *
     */
    private Integer questionIsVolunteerAssigned;

    /**
     *
     */
    private String currentDisputeId;

    /**
     *
     */
    private Date projectCreateTime;

    /**
     *
     */
    private Integer donationPoolLimit;

    /**
     *
     */
    private Integer currentDonationsReceived;

    /**
     *
     */
    private Integer currentUsedDonations;

    /**
     *
     */
    private Date lastUpdateTime;

    /**
     *
     */
    private String pathography;

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
        Project other = (Project) that;
        return (this.getProjectId() == null ? other.getProjectId() == null : this.getProjectId().equals(other.getProjectId()))
                && (this.getUserName() == null ? other.getUserName() == null : this.getUserName().equals(other.getUserName()))
                && (this.getUserAge() == null ? other.getUserAge() == null : this.getUserAge().equals(other.getUserAge()))
                && (this.getRealName() == null ? other.getRealName() == null : this.getRealName().equals(other.getRealName()))
                && (this.getProjectStatus() == null ? other.getProjectStatus() == null : this.getProjectStatus().equals(other.getProjectStatus()))
                && (this.getVerificationRecord() == null ? other.getVerificationRecord() == null
                                                         : this.getVerificationRecord().equals(other.getVerificationRecord()))
                && (this.getCurrentVolunteer() == null ? other.getCurrentVolunteer() == null
                                                       : this.getCurrentVolunteer().equals(other.getCurrentVolunteer()))
                && (this.getPreviousVolunteers() == null ? other.getPreviousVolunteers() == null
                                                         : this.getPreviousVolunteers().equals(other.getPreviousVolunteers()))
                && (this.getQuestionIsVolunteerAssigned() == null ? other.getQuestionIsVolunteerAssigned() == null
                                                                  : this.getQuestionIsVolunteerAssigned()
                            .equals(other.getQuestionIsVolunteerAssigned()))
                && (this.getCurrentDisputeId() == null ? other.getCurrentDisputeId() == null
                                                       : this.getCurrentDisputeId().equals(other.getCurrentDisputeId()))
                && (this.getProjectCreateTime() == null ? other.getProjectCreateTime() == null
                                                        : this.getProjectCreateTime().equals(other.getProjectCreateTime()))
                && (this.getDonationPoolLimit() == null ? other.getDonationPoolLimit() == null
                                                        : this.getDonationPoolLimit().equals(other.getDonationPoolLimit()))
                && (this.getCurrentDonationsReceived() == null ? other.getCurrentDonationsReceived() == null
                                                               : this.getCurrentDonationsReceived().equals(other.getCurrentDonationsReceived()))
                && (this.getCurrentUsedDonations() == null ? other.getCurrentUsedDonations() == null
                                                           : this.getCurrentUsedDonations().equals(other.getCurrentUsedDonations()))
                && (this.getLastUpdateTime() == null ? other.getLastUpdateTime() == null : this.getLastUpdateTime().equals(other.getLastUpdateTime()))
                && (this.getPathography() == null ? other.getPathography() == null : this.getPathography().equals(other.getPathography()));
    }

    @Override
    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((getProjectId() == null) ? 0 : getProjectId().hashCode());
        result = prime * result + ((getUserName() == null) ? 0 : getUserName().hashCode());
        result = prime * result + ((getUserAge() == null) ? 0 : getUserAge().hashCode());
        result = prime * result + ((getRealName() == null) ? 0 : getRealName().hashCode());
        result = prime * result + ((getProjectStatus() == null) ? 0 : getProjectStatus().hashCode());
        result = prime * result + ((getVerificationRecord() == null) ? 0 : getVerificationRecord().hashCode());
        result = prime * result + ((getCurrentVolunteer() == null) ? 0 : getCurrentVolunteer().hashCode());
        result = prime * result + ((getPreviousVolunteers() == null) ? 0 : getPreviousVolunteers().hashCode());
        result = prime * result + ((getQuestionIsVolunteerAssigned() == null) ? 0 : getQuestionIsVolunteerAssigned().hashCode());
        result = prime * result + ((getCurrentDisputeId() == null) ? 0 : getCurrentDisputeId().hashCode());
        result = prime * result + ((getProjectCreateTime() == null) ? 0 : getProjectCreateTime().hashCode());
        result = prime * result + ((getDonationPoolLimit() == null) ? 0 : getDonationPoolLimit().hashCode());
        result = prime * result + ((getCurrentDonationsReceived() == null) ? 0 : getCurrentDonationsReceived().hashCode());
        result = prime * result + ((getCurrentUsedDonations() == null) ? 0 : getCurrentUsedDonations().hashCode());
        result = prime * result + ((getLastUpdateTime() == null) ? 0 : getLastUpdateTime().hashCode());
        result = prime * result + ((getPathography() == null) ? 0 : getPathography().hashCode());
        return result;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(getClass().getSimpleName());
        sb.append(" [");
        sb.append("Hash = ").append(hashCode());
        sb.append(", projectId=").append(projectId);
        sb.append(", userName=").append(userName);
        sb.append(", userAge=").append(userAge);
        sb.append(", realName=").append(realName);
        sb.append(", projectStatus=").append(projectStatus);
        sb.append(", verificationRecord=").append(verificationRecord);
        sb.append(", currentVolunteer=").append(currentVolunteer);
        sb.append(", previousVolunteers=").append(previousVolunteers);
        sb.append(", questionIsVolunteerAssigned=").append(questionIsVolunteerAssigned);
        sb.append(", currentDisputeId=").append(currentDisputeId);
        sb.append(", projectCreateTime=").append(projectCreateTime);
        sb.append(", donationPoolLimit=").append(donationPoolLimit);
        sb.append(", currentDonationsReceived=").append(currentDonationsReceived);
        sb.append(", currentUsedDonations=").append(currentUsedDonations);
        sb.append(", lastUpdateTime=").append(lastUpdateTime);
        sb.append(", pathography=").append(pathography);
        sb.append(", serialVersionUID=").append(serialVersionUID);
        sb.append("]");
        return sb.toString();
    }
}