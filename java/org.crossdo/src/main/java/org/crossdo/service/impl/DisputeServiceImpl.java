package org.crossdo.service.impl;

import org.crossdo.entity.Dispute;
import org.crossdo.mapper.DisputeMapper;
import org.crossdo.service.DisputeService;
import org.springframework.stereotype.Service;
import org.springframework.context.annotation.Profile;

import com.baomidou.mybatisplus.extension.service.impl.ServiceImpl;

/**
 * @author dollydays
 * @description 针对表【dispute】的数据库操作Service实现
 * @createDate 2023-11-29 11:13:09
 */
@Service
@Profile("direct-db")
public class DisputeServiceImpl extends ServiceImpl<DisputeMapper, Dispute>
        implements DisputeService {

}
