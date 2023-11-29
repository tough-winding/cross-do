package org.crossdo.entity;

import java.io.Serializable;

import com.baomidou.mybatisplus.annotations.TableField;
import com.baomidou.mybatisplus.annotations.TableId;
import com.baomidou.mybatisplus.annotations.TableName;

import lombok.Data;

/**
 * @TableName dispute
 */
@TableName(value = "dispute")
@Data
public class Dispute implements Serializable {
    /**
     *
     */
    @TableId
    private String currentDisputeId;

    /**
     *
     */
    private Integer disputeTypes;

    /**
     *
     */
    private String userId;

    /**
     *
     */
    private String projectId;

    /**
     *
     */
    private Integer disputeStatus;

    /**
     *
     */
    private String reason;

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
        Dispute other = (Dispute) that;
        return (this.getCurrentDisputeId() == null ? other.getCurrentDisputeId() == null
                                                   : this.getCurrentDisputeId().equals(other.getCurrentDisputeId()))
                && (this.getDisputeTypes() == null ? other.getDisputeTypes() == null : this.getDisputeTypes().equals(other.getDisputeTypes()))
                && (this.getUserId() == null ? other.getUserId() == null : this.getUserId().equals(other.getUserId()))
                && (this.getProjectId() == null ? other.getProjectId() == null : this.getProjectId().equals(other.getProjectId()))
                && (this.getDisputeStatus() == null ? other.getDisputeStatus() == null : this.getDisputeStatus().equals(other.getDisputeStatus()))
                && (this.getReason() == null ? other.getReason() == null : this.getReason().equals(other.getReason()));
    }

    @Override
    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((getCurrentDisputeId() == null) ? 0 : getCurrentDisputeId().hashCode());
        result = prime * result + ((getDisputeTypes() == null) ? 0 : getDisputeTypes().hashCode());
        result = prime * result + ((getUserId() == null) ? 0 : getUserId().hashCode());
        result = prime * result + ((getProjectId() == null) ? 0 : getProjectId().hashCode());
        result = prime * result + ((getDisputeStatus() == null) ? 0 : getDisputeStatus().hashCode());
        result = prime * result + ((getReason() == null) ? 0 : getReason().hashCode());
        return result;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(getClass().getSimpleName());
        sb.append(" [");
        sb.append("Hash = ").append(hashCode());
        sb.append(", currentDisputeId=").append(currentDisputeId);
        sb.append(", disputeTypes=").append(disputeTypes);
        sb.append(", userId=").append(userId);
        sb.append(", projectId=").append(projectId);
        sb.append(", disputeStatus=").append(disputeStatus);
        sb.append(", reason=").append(reason);
        sb.append(", serialVersionUID=").append(serialVersionUID);
        sb.append("]");
        return sb.toString();
    }
}