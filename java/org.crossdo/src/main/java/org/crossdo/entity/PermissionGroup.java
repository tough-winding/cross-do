package org.crossdo.entity;

import java.io.Serializable;

import com.baomidou.mybatisplus.annotations.TableField;
import com.baomidou.mybatisplus.annotations.TableId;
import com.baomidou.mybatisplus.annotations.TableName;

import lombok.Data;

/**
 * @TableName permission_group
 */
@TableName(value = "permission_group")
@Data
public class PermissionGroup implements Serializable {
    /**
     *
     */
    @TableId
    private Integer id;

    /**
     *
     */
    private String groupName;

    /**
     *
     */
    private String groupIllustrate;

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
        PermissionGroup other = (PermissionGroup) that;
        return (this.getId() == null ? other.getId() == null : this.getId().equals(other.getId()))
                && (this.getGroupName() == null ? other.getGroupName() == null : this.getGroupName().equals(other.getGroupName()))
                && (this.getGroupIllustrate() == null ? other.getGroupIllustrate() == null
                                                      : this.getGroupIllustrate().equals(other.getGroupIllustrate()));
    }

    @Override
    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((getId() == null) ? 0 : getId().hashCode());
        result = prime * result + ((getGroupName() == null) ? 0 : getGroupName().hashCode());
        result = prime * result + ((getGroupIllustrate() == null) ? 0 : getGroupIllustrate().hashCode());
        return result;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(getClass().getSimpleName());
        sb.append(" [");
        sb.append("Hash = ").append(hashCode());
        sb.append(", id=").append(id);
        sb.append(", groupName=").append(groupName);
        sb.append(", groupIllustrate=").append(groupIllustrate);
        sb.append(", serialVersionUID=").append(serialVersionUID);
        sb.append("]");
        return sb.toString();
    }
}