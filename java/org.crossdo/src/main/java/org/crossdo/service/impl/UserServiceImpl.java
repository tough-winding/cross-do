package org.crossdo.service.impl;

import org.crossdo.entity.User;
import org.crossdo.mapper.UserMapper;
import org.crossdo.service.UserService;
import org.springframework.stereotype.Service;
import org.springframework.context.annotation.Profile;

import com.baomidou.mybatisplus.extension.service.impl.ServiceImpl;

/**
 * @author dollydays
 * @description 针对表【user】的数据库操作Service实现
 * @createDate 2023-11-29 11:13:09
 */
@Service
@Profile("direct-db")
public class UserServiceImpl extends ServiceImpl<UserMapper, User>
        implements UserService {

}
