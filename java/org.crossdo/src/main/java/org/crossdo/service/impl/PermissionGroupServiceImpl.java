package org.crossdo.service.impl;

import org.crossdo.entity.PermissionGroup;
import org.crossdo.mapper.PermissionGroupMapper;
import org.crossdo.service.PermissionGroupService;
import org.springframework.stereotype.Service;
import org.springframework.context.annotation.Profile;

import com.baomidou.mybatisplus.extension.service.impl.ServiceImpl;

/**
 * @author dollydays
 * @description 针对表【permission_group】的数据库操作Service实现
 * @createDate 2023-11-29 11:13:09
 */
@Service
@Profile("direct-db")
public class PermissionGroupServiceImpl extends ServiceImpl<PermissionGroupMapper, PermissionGroup>
        implements PermissionGroupService {

}
