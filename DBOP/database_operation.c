// 编译命令： gcc -o database_operation.exe database_operation.c -levent -ljansson -lzlog -lyaml -luuid -L/usr/lib64/mysql -lhiredis -lmysqlclient -lpthread -std=c99 -O3

// 整体目的：控制数据库的增删改查操作，均由该服务提供，对外发布socket服务
/*
    访问接口样例及返回体样例
    REQUEST：   查询service对应的service密码
                curl -X POST "http://服务ip:服务端口/auth/get_service_passwd" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"service_name":"分配的服务名"}'
                curl -X POST "http://192.168.1.10:1900/auth/get_service_passwd" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"service_username":"ftp"}'
    200RETURN： {"ServiceName":"ftp", "ServicePassword":"anotherlongpassword987654"}  
    REQUEST：   在user表中添加用户
                curl -X POST "http://服务ip:服务端口/user/add" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "user_name":"用户名", "user_passwd":"用户密码字符串", "user_permission":账号分类数字, "phone_number":"用户手机号"}'
                curl -X POST "http://192.168.1.10:1900/user/add" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d {"user_id":"d7aa0e59-9be5-4c76-9bf1-ee937791b8a8", "user_name":"两茫茫", "user_passwd":"qwe1j31l23nubadbuiy13", "user_permission":3, "phone_number":"18212312310"}'
    200RETURN： 什么都不返回
    REQUEST:    更新最后一次登录时间
                curl -X POST "http://服务ip:服务端口/user/update_login_time" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID"}'
                curl -X POST "http://192.168.1.10:1900/user/update_login_time" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db"}'
    200RETURN： 什么都不返回
    REQUEST:    更新用户个性化信息
                curl -X POST "http://服务ip:服务端口/user/update_info" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "user_name":"用户名", "avatar":"用户头像路径", "personal_note": "个人说明"}'
                curl -X POST "http://192.168.1.10:1900/user/update_info" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "user_name":"李通", "avatar":"https://www.test.com/img/flexible/logo/pc/result.png", "personal_note": "生死两茫茫"}'
    200RETURN： 什么都不返回
    REQUEST:    更新用户手机号
                curl -X POST "http://服务ip:服务端口/user/update_phone" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "phone_number":"用户手机号"}'
                curl -X POST "http://192.168.1.10:1900/user/update_phone" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "phone_number": "19212345678"}'
    200RETURN： 什么都不返回
    REQUEST:    更新用户微信ID
                curl -X POST "http://服务ip:服务端口/user/update_weixin" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "weixin_openid":"用户微信OpenID"}'
                curl -X POST "http://192.168.1.10:1900/user/update_weixin" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "weixin_openid": "4fcb6538-9b7e-4f25-8a01-47016843b7a6"}'
    200RETURN： 什么都不返回
    REQUEST:    更新用户权限组
                curl -X POST "http://服务ip:服务端口/user/update_permission" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "user_permission":账号分类数字}'
                curl -X POST "http://192.168.1.10:1900/user/update_permission" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "user_permission": 1}'
    200RETURN： 什么都不返回
    REQUEST:    修改用户密码
                curl -X POST "http://服务ip:服务端口/user/update_password" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "passwd":"用户密码字符串"}'
                curl -X POST "http://192.168.1.10:1900/user/update_password" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "passwd": "kojiqjwe1312uj2"}'
    200RETURN： 什么都不返回
    REQUEST:    创建项目
                curl -X POST "http://服务ip:服务端口/project/create" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "user_id":"用户ID", "user_name":"用户名", "user_age":"用户年龄", "real_name":"用户姓名"}'
                curl -X POST "http://192.168.1.10:1900/project/create" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "user_name":"李通", "user_age":67, "real_name":"李通达"}'
    200RETURN： 什么都不返回
    REQUEST:    更新验证记录
                curl -X POST "http://服务ip:服务端口/project/update_verification_record" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "update_verification_record":"消息json"}'
                curl -X POST "http://192.168.1.10:1900/project/update_verification_record" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "update_verification_record":"[{"patient":{"我在遥望":"请志愿者看下这次的资料合格了吗？","attachment":["https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/病历初稿.pdf","https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/检查报告.jpg"],"time":"2025-05-27T14:00:00+08:00"},"volunteer":{"bceac":"您的资料还存在以下问题：病历不完整，没有医生签字证明。请补充好之后再提交一下。","attachment":[],"time":"2025-05-27T14:15:00+08:00"}},{"patient":{"我在遥望":"我又调整了一下，您再看下呢？","attachment":["https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/补充病历.pdf","https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/医生签字页.png"],"time":"2025-05-27T15:00:00+08:00"},"volunteer":{"bceac":"这次可以了，给您通过了。","attachment":[],"time":"2025-05-27T15:10:00+08:00"}}]"}'
    200RETURN： 什么都不返回
    REQUEST:    更新项目状态
                curl -X POST "http://服务ip:服务端口/project/update_status" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "projest_status":项目状态码}'
                curl -X POST "http://192.168.1.10:1900/project/update_status" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "projest_status":1}'
    200RETURN： 什么都不返回
    REQUEST:    更新病情记录
                curl -X POST "http://服务ip:服务端口/project/update_pathography" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "pathography":"记录JSON"}'
                curl -X POST "http://192.168.1.10:1900/project/update_pathography" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "pathography":"[{"time":"2020-01-05 14:03:01","content":"刚刚确诊为***，心情很是沉重，医生说需要******来治疗","attachment":["minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/20200105140301-1.jpg","minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/20200105140301-2.jpg"]},{"time":"2020-01-15 12:25:31","content":"这一周多的治疗效果好多了，现在又有点希望了","attachment":["minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/20200115122531-1.jpg","minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/20200115122531-2.jpg"]}]"}'
    200RETURN： 什么都不返回
    REQUEST:    更新项目负责志愿者ID
                curl -X POST "http://服务ip:服务端口/project/update_volunteer" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "volunteer_id":"志愿者ID"}'
                curl -X POST "http://192.168.1.10:1900/project/update_volunteer" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "volunteer_id":"8f76fa9c-18d2-41a9-bdd6-6c8e4c489872"}'
    200RETURN： 什么都不返回
    REQUEST:    新增捐款记录
                curl -X POST "http://服务ip:服务端口/ledger/create_donation" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID", "project_id":"项目ID", "user_id":"捐赠者ID", "sufferer_user_id":"项目受助者ID", "donor_user_name":"捐赠者用户名", "sufferer_real_name":"患者真实姓名", "sufferer_user_name":"患者用户名", "amount":"捐赠金额", "note":"备注", "payment_method":"捐款渠道", "method_id":"渠道账单ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/create_donation" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"ledger_id":"eaf8ef43-b186-4e68-83bb-cdf0a3c490df", "project_id":"a7c16b7d-bc2e-4e2e-8617-527cfb23d29f", "user_id":"4a93cd46-f109-4a89-9256-2dc2b3cc4dc4", "sufferer_user_id":"d17d43db-121a-4b9e-97d6-80a3b00ae062", "donor_user_name":"张三心", "sufferer_real_name":"李四", "sufferer_user_name":"木子四", "amount":"100", "note":"望康复", "payment_method":0, "method_id":"2024060122001400221404567890"}'
    200RETURN： 什么都不返回
    REQUEST: 将捐赠类型的账单状态转为支付失败
                curl -X POST "http://服务ip:服务端口/ledger/set_status_payment_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_payment_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"eaf8ef43-b186-4e68-83bb-cdf0a3c490df"}'
    200RETURN：什么都不返回
    REQUEST: 将捐赠类型的账单状态转为处理中
                curl -X POST "http://服务ip:服务端口/ledger/set_status_payment_processing" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_payment_processing" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"eaf8ef43-b186-4e68-83bb-cdf0a3c490df"}'
    200RETURN：什么都不返回
    REQUEST: 将捐赠类型的账单状态转为处理失败
                curl -X POST "http://服务ip:服务端口/ledger/set_status_payment_process_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_payment_process_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"eaf8ef43-b186-4e68-83bb-cdf0a3c490df"}'
    200RETURN：什么都不返回
    REQUEST: 将捐赠类型的账单状态转为退款中
                curl -X POST "http://服务ip:服务端口/ledger/set_status_payment_refunding" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_payment_refunding" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"eaf8ef43-b186-4e68-83bb-cdf0a3c490df"}'
    200RETURN：什么都不返回
    REQUEST: 将捐赠类型的账单状态转为退款失败
                curl -X POST "http://服务ip:服务端口/ledger/set_status_payment_refund_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_payment_refund_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"eaf8ef43-b186-4e68-83bb-cdf0a3c490df"}'
    200RETURN：什么都不返回
    REQUEST: 将捐赠类型的账单状态转为退款完成
                curl -X POST "http://服务ip:服务端口/ledger/set_status_payment_refund_completed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_payment_refund_completed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"eaf8ef43-b186-4e68-83bb-cdf0a3c490df"}'
    200RETURN：什么都不返回
    REQUEST: 将捐赠类型的账单状态转为完成
                curl -X POST "http://服务ip:服务端口/ledger/set_status_payment_completed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_payment_completed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"eaf8ef43-b186-4e68-83bb-cdf0a3c490df"}'
    200RETURN：什么都不返回
    REQUEST: 新增退款待选择记录
                curl -X POST "http://服务ip:服务端口/ledger/create_refund_pending_selection" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID", "project_id":"项目ID", "user_id":"系统用户ID", "sufferer_user_id":"项目受助者ID", "donor_user_name":"捐赠者用户名", "sufferer_real_name":"患者真实姓名", "sufferer_user_name":"患者用户名", "amount":"退款金额", "target_user_id":"退款目标用户ID", "note":"备注"}'
                curl -X POST "http://192.168.1.10:1900/ledger/create_refund_pending_selection" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b", "project_id":"a7c16b7d-bc2e-4e2e-8617-527cfb23d29f", "user_id":"4647baa3-8af6-4dca-90c1-30e5ddbca112", "sufferer_user_id":"d17d43db-121a-4b9e-97d6-80a3b00ae062", "donor_user_name":"张三心", "sufferer_real_name":"李四", "sufferer_user_name":"木子四", "amount":"-100", "target_user_id":"4a93cd46-f109-4a89-9256-2dc2b3cc4dc4", "note":"项目已结束，剩余项目款项300元，按捐款比例退还您100元，请选择退款方式或转入应急资金池。"}'
    200RETURN：什么都不返回
    REQUEST: 将退款待选择类型的记录性质转为退款类型
                curl -X POST "http://服务ip:服务端口/ledger/set_type_refund" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_type_refund" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b"}'
    200RETURN：什么都不返回
    REQUEST: 将退款类型的账单状态转为退款失败
                curl -X POST "http://服务ip:服务端口/ledger/set_status_refund_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_refund_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b"}'
    200RETURN：什么都不返回
    REQUEST: 将退款类型的账单状态转为退款完成
                curl -X POST "http://服务ip:服务端口/ledger/set_status_refund_completed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_refund_completed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b"}'
    200RETURN：什么都不返回
    REQUEST: 将退款待选择类型的记录性质转为转应急池类型
                curl -X POST "http://服务ip:服务端口/ledger/set_type_emergency_pool" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_type_emergency_pool" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b"}'
    200RETURN：什么都不返回
    REQUEST: 新增患者使用记录
                curl -X POST "http://服务ip:服务端口/ledger/create_patient_usage" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID", "project_id":"项目ID", "user_id":"患者用户ID", "sufferer_user_id":"项目受助者ID", "sufferer_real_name":"患者真实姓名", "sufferer_user_name":"患者用户名", "amount":"使用金额", "payment_method":"使用渠道", "note":"备注", "verification_record":"审计json"}'
                curl -X POST "http://192.168.1.10:1900/ledger/create_patient_usage" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b", "project_id":"a7c16b7d-bc2e-4e2e-8617-527cfb23d29f", "user_id":"4647baa3-8af6-4dca-90c1-30e5ddbca112", "sufferer_user_id":"d17d43db-121a-4b9e-97d6-80a3b00ae062", "sufferer_real_name":"李四", "sufferer_user_name":"木子四", "amount":"-100", "payment_method":0, "note":"化验费用报销申请", "verification_record":"[{"patient":{"我在遥望":"化验费用报销申请","attachment":["https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/医保结算单.pdf","https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/发票.jpg"],"time":"2025-06-29T11:12:00+08:00"},"volunteer":{"bceac":"没什么问题，给您通过了。","attachment":[],"time":"2025-06-29T11:20:00+08:00"}}]"}'
    200RETURN：什么都不返回
    REQUEST: 将患者使用类型的记录性质转为处理失败类型
                curl -X POST "http://服务ip:服务端口/ledger/set_status_patient_usage_process_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_patient_usage_process_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b"}'
    200RETURN：什么都不返回
    REQUEST: 将患者使用类型的记录性质转为发款中类型
                curl -X POST "http://服务ip:服务端口/ledger/set_status_patient_usage_paying" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_patient_usage_paying" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b"}'
    200RETURN：什么都不返回
    REQUEST: 将患者使用类型的记录性质转为发款失败类型
                curl -X POST "http://服务ip:服务端口/ledger/set_status_patient_usage_pay_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_patient_usage_pay_failed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b"}'
    200RETURN：什么都不返回
    REQUEST: 将患者使用类型的记录性质转为完成类型
                curl -X POST "http://服务ip:服务端口/ledger/set_status_patient_usage_completed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID"}'
                curl -X POST "http://192.168.1.10:1900/ledger/set_status_patient_usage_completed" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b"}'
    200RETURN：什么都不返回
    REQUEST: 新增患者应急使用记录
                curl -X POST "http://服务ip:服务端口/ledger/create_emergency_usage" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"ledger_id":"记录ID", "project_id":"项目ID", "user_id":"患者用户ID", "sufferer_user_id":"项目受助者ID", "sufferer_real_name":"患者真实姓名", "sufferer_user_name":"患者用户名", "amount":"使用金额", "payment_method":"使用渠道", "note":"备注"}'
                curl -X POST "http://192.168.1.10:1900/ledger/create_emergency_usage" -H "ServiceName: IAM_SERVICE" -H "Authorization: 服务token" -H "Content-Type: application/json" -d '{"ledger_id":"5206f237-f070-48a9-8f4d-9eb15e31c21b", "project_id":"a7c16b7d-bc2e-4e2e-8617-527cfb23d29f", "user_id":"4647baa3-8af6-4dca-90c1-30e5ddbca112", "sufferer_user_id":"d17d43db-121a-4b9e-97d6-80a3b00ae062", "sufferer_real_name":"李四", "sufferer_user_name":"木子四", "amount":"-100", "payment_method":0, "note":"化验费用报销申请"}'
    200RETURN：什么都不返回
*/
/*
变量命名要求如下：
    函数        模块名缩写(全大写)_FUN_函数名(大驼峰命名)
    常量        模块名缩写(全大写)_CON_常量名(小驼峰命名)
    全局变量    模块名缩写(全大写)_GLV_变量名(小驼峰命名)
    变量        模块名缩写(全大写)_VAR_函数名(大驼峰命名)_变量名(小驼峰命名)
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>
#include <mysql/mysql.h>
#include <time.h>
#include <zlog.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <stdbool.h>
#include <jansson.h>
#include <stdarg.h>
#include <uuid/uuid.h>
#include <hiredis/hiredis.h>
#include <pthread.h>

pthread_mutex_t DBOP_GLV_redisConnection_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t DBOP_GLV_mysqlConnection_mutex = PTHREAD_MUTEX_INITIALIZER;

#define HTTP_OK 200
#define HTTP_FORBIDDEN 403
#define HTTP_INTERNAL 500
#define MAX_DB_POOL_SIZE 100  // 定义最大数据库连接池大小

// 存储实际使用的连接池大小
int DBOP_GLV_actualDBPoolSize = 10;  // 默认值为10
int DBOP_GLV_actualRedisPoolSize = 10;  // 默认值为10

// 声明结构体，用以存储常量
typedef struct {
    char *DBOP_GLV_serverPort;
    char *DBOP_GLV_logFile;
    char *DBOP_GLV_logConfig;
    char *DBOP_GLV_dbIp;
    char *DBOP_GLV_dbServerPort;
    char *DBOP_GLV_dbPrivilegeUser;
    char *DBOP_GLV_dbPrivilegePassword;
    char *DBOP_GLV_dbName;
    char* DBOP_GLV_redisServerIp;
    char* DBOP_GLV_redisServerPort;
    char* DBOP_GLV_redisServerPassword;
    char* DBOP_GLV_databasePoolSize;  // 数据库连接池大小
    char* DBOP_GLV_redisPoolSize;     // Redis连接池大小
} AppConfig;

// 预处理语句句柄结构体
typedef struct {
    MYSQL *mysql;
    MYSQL_STMT *stmt_check_phone;
    MYSQL_STMT *stmt_add_user;
    MYSQL_STMT *stmt_get_service_passwd;
    MYSQL_STMT *stmt_add_project;
    MYSQL_STMT *stmt_update_login_time;
    MYSQL_STMT *stmt_update_user_info;
    MYSQL_STMT *stmt_update_user_phone;
    MYSQL_STMT *stmt_update_user_weixinid;
    MYSQL_STMT *stmt_update_user_permission;
    MYSQL_STMT *stmt_update_user_passwd;
    MYSQL_STMT *stmt_update_user_project_info;
    MYSQL_STMT *stmt_update_verification_record;
    MYSQL_STMT *stmt_update_project_status;
    MYSQL_STMT *stmt_update_project_pathography;
    MYSQL_STMT *stmt_update_project_volunteer;
    MYSQL_STMT *stmt_get_project_volunteer_info;
    MYSQL_STMT *stmt_make_donation;
    MYSQL_STMT *stmt_set_status_payment_failed;
    MYSQL_STMT *stmt_set_status_payment_processing;
    MYSQL_STMT *stmt_set_status_payment_process_failed;
    MYSQL_STMT *stmt_set_status_payment_refunding;
    MYSQL_STMT *stmt_set_status_payment_refund_failed;
    MYSQL_STMT *stmt_set_status_payment_refund_completed;
    MYSQL_STMT *stmt_set_status_payment_completed;
    MYSQL_STMT *stmt_create_refund_pending_selection;
    MYSQL_STMT *stmt_set_type_refund;
    MYSQL_STMT *stmt_set_status_refund_failed;
    MYSQL_STMT *stmt_set_status_refund_completed;
    MYSQL_STMT *stmt_set_type_emergency_pool;
    MYSQL_STMT *stmt_create_patient_usage;
    MYSQL_STMT *stmt_create_volunteer_audit;
    MYSQL_STMT *stmt_create_emergency_usage;
    MYSQL_STMT *stmt_check_pending_emergency_usage;
    MYSQL_STMT *stmt_set_status_patient_usage_process_failed;
    MYSQL_STMT *stmt_set_status_patient_usage_paying;
    MYSQL_STMT *stmt_set_status_patient_usage_pay_failed;
    MYSQL_STMT *stmt_set_status_patient_usage_completed;
} DB_CONNECTION;

// 初始化dzlog
void DBOP_FUN_InitLogging(AppConfig *DBOP_VAR_InitLogging_cfg) {
    int rc;
    rc = dzlog_init(DBOP_VAR_InitLogging_cfg->DBOP_GLV_logConfig, "whatever");
    if (rc) {
        fprintf(stderr, "zlog init failed\n");
        exit(EXIT_FAILURE);
    }
}

// DBOP_FUN_MainConfigParse函数作用是读取配置文件，并赋值
AppConfig DBOP_FUN_MainConfigParse(const char* DBOP_VAR_MainConfigParse_fileName) {
    FILE *DBOP_VAR_MainConfigParse_fileHandle = fopen(DBOP_VAR_MainConfigParse_fileName, "r");
    yaml_parser_t DBOP_VAR_MainConfigParse_yamlParser;
    yaml_event_t DBOP_VAR_MainConfigParse_yamlEvent;

    // 初始化yaml解析器
    if (!yaml_parser_initialize(&DBOP_VAR_MainConfigParse_yamlParser)) {
        fprintf(stderr, "Failed to initialize DBOP_VAR_MainConfigParse_yamlParser\n");
        exit(EXIT_FAILURE);
    }
    if (DBOP_VAR_MainConfigParse_fileHandle == NULL) {
        fprintf(stderr, "Failed to open file\n");
        exit(EXIT_FAILURE);
    }

    // 设置输入文件
    yaml_parser_set_input_file(&DBOP_VAR_MainConfigParse_yamlParser, DBOP_VAR_MainConfigParse_fileHandle);

    AppConfig DBOP_VAR_MainConfigParse_mainConfig = {0};
    char* DBOP_VAR_MainConfigParse_currentKey = NULL;

    // 解析YAML文件
    bool done = false;
    while (!done) {
        if (!yaml_parser_parse(&DBOP_VAR_MainConfigParse_yamlParser, &DBOP_VAR_MainConfigParse_yamlEvent)) {
            fprintf(stderr, "Parser error\n");
            exit(EXIT_FAILURE);
        }

        switch(DBOP_VAR_MainConfigParse_yamlEvent.type) {
        case YAML_SCALAR_EVENT:
            if (DBOP_VAR_MainConfigParse_currentKey) {
                if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "server_port") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_serverPort = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "log_file") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_logFile = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "log_config") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_logConfig = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "database_ip") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbIp = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "database_port") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbServerPort = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "database_privilege_user") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbPrivilegeUser = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "database_privilege_password") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbPrivilegePassword = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "user_database_name") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbName = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "database_pool_size") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_databasePoolSize = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "redis_server") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerIp = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "redis_port") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPort = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "redis_password") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPassword = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "redis_pool_size") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisPoolSize = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                }
                free(DBOP_VAR_MainConfigParse_currentKey);
                DBOP_VAR_MainConfigParse_currentKey = NULL;
            } else {
                DBOP_VAR_MainConfigParse_currentKey = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
            }
            break;
        case YAML_STREAM_END_EVENT:
            done = true;
            break;
        default:
            break;
        }

        // 释放事件
        yaml_event_delete(&DBOP_VAR_MainConfigParse_yamlEvent);
    }

    // 释放yaml解析器
    yaml_parser_delete(&DBOP_VAR_MainConfigParse_yamlParser);
    fclose(DBOP_VAR_MainConfigParse_fileHandle);

    // 检查每个必需的配置项是否已经被正确设置
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_serverPort) dzlog_error("Missing configuration: server_port");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_logFile) dzlog_error("Missing configuration: log_file");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_logConfig) dzlog_error("Missing configuration: log_config");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbIp) dzlog_error("Missing configuration: database_ip");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbServerPort) dzlog_error("Missing configuration: database_port");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbPrivilegeUser) dzlog_error("Missing configuration: database_privilege_user");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbPrivilegePassword) dzlog_error("Missing configuration: database_privilege_password");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbName) dzlog_error("Missing configuration: user_database_name.");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_databasePoolSize) dzlog_error("Missing configuration: database_pool_size.");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerIp) dzlog_error("Missing configuration: redis_ip.");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPort) dzlog_error("Missing configuration: redis_port.");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPassword) dzlog_error("Missing configuration: redis_password.");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisPoolSize) dzlog_error("Missing configuration: redis_pool_size.");

    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_serverPort ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_logFile ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_logConfig ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbIp ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbServerPort ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbPrivilegeUser ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbPrivilegePassword ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbName ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_databasePoolSize ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerIp ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPort ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPassword ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisPoolSize) {
        exit(EXIT_FAILURE);
    }

    return DBOP_VAR_MainConfigParse_mainConfig;
}


// 释放配置文件变量
void DBOP_FUN_FreeConfig(AppConfig *DBOP_VAR_FreeConfig_cfg) {
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_serverPort);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_dbIp);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_dbServerPort);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_dbPrivilegeUser);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_dbPrivilegePassword);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_logFile);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_dbName);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_databasePoolSize);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_redisServerIp);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_redisServerPort);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_redisServerPassword);
    free(DBOP_VAR_FreeConfig_cfg->DBOP_GLV_redisPoolSize);
}


// ------------------------redis操作逻辑开始----------------------------


// 定义redis连接池
typedef struct {
    redisContext** DBOP_GLV_redisConnections; // 指向Redis连接的指针数组
    int DBOP_GLV_poolSize;
} RedisPool;

RedisPool* DBOP_GLV_redisConnectPool = NULL;

// 初始化和连接到Redis
// 正确返回为redis_connect的连接，错误返回为NULL
redisContext* DBOP_FUN_InitializeRedis(const AppConfig* DBOP_VAR_InitializeRedis_config) {
    char* endptr;
    long DBOP_VAR_InitializeRedis_redisPort = strtol(DBOP_VAR_InitializeRedis_config->DBOP_GLV_redisServerPort, &endptr, 10);
    if (*endptr != '\0') {
        dzlog_error("Invalid port number: Non-numeric characters present.");
        return NULL;
    }
    redisContext *DBOP_VAR_InitializeRedis_redisConnect = redisConnect(DBOP_VAR_InitializeRedis_config->DBOP_GLV_redisServerIp, (int)DBOP_VAR_InitializeRedis_redisPort);
    if (DBOP_VAR_InitializeRedis_redisConnect == NULL || DBOP_VAR_InitializeRedis_redisConnect->err) {
        if (DBOP_VAR_InitializeRedis_redisConnect) {
            dzlog_error("Redis connection error: %s.", DBOP_VAR_InitializeRedis_redisConnect->errstr);
            redisFree(DBOP_VAR_InitializeRedis_redisConnect);
        } else {
            dzlog_error("Connection error: can't allocate redis context.");
        }
        return NULL;
    }

    // 如果提供了密码，则使用它进行认证
    if (DBOP_VAR_InitializeRedis_config->DBOP_GLV_redisServerPassword != NULL && strlen(DBOP_VAR_InitializeRedis_config->DBOP_GLV_redisServerPassword) > 0) {
        redisReply *reply = redisCommand(DBOP_VAR_InitializeRedis_redisConnect, "AUTH %s", DBOP_VAR_InitializeRedis_config->DBOP_GLV_redisServerPassword);
        if (reply->type == REDIS_REPLY_ERROR) {
            dzlog_error("Auth error: %s.", reply->str);
            freeReplyObject(reply);
            redisFree(DBOP_VAR_InitializeRedis_redisConnect);
            return NULL;
        }
        freeReplyObject(reply);
    }

    return DBOP_VAR_InitializeRedis_redisConnect;
}

// 创建redis资源池
RedisPool* DBOP_FUN_InitializeRedisPool(const AppConfig* config, int poolSize) {
    RedisPool* DBOP_VAR_InitializeRedisPool_pool = (RedisPool*)malloc(sizeof(RedisPool));
    DBOP_VAR_InitializeRedisPool_pool->DBOP_GLV_redisConnections = (redisContext**)malloc(sizeof(redisContext*) * poolSize);
    DBOP_VAR_InitializeRedisPool_pool->DBOP_GLV_poolSize = poolSize;

    for (int i = 0; i < poolSize; i++) {
        DBOP_VAR_InitializeRedisPool_pool->DBOP_GLV_redisConnections[i] = DBOP_FUN_InitializeRedis(config);
    }

    return DBOP_VAR_InitializeRedisPool_pool;
}


// 检查redis是否可用，如果不可用则重新初始化redis连接
// 正常返回true，异常且重新初始化redis连接失败则停止服务运行
bool DBOP_FUN_CheckAndReinitializeRedis(const AppConfig* DBOP_VAR_CheckAndReinitializeRedis_config, int DBOP_VAR_CheckAndReinitializeRedis_index) {
    // 使用一个简单的命令来检查Redis连接
    if (DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index] != NULL) {
        redisReply *reply = redisCommand(DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index], "PING");
        if (reply != NULL && reply->type != REDIS_REPLY_ERROR) {
            freeReplyObject(reply);
            return true;
        }
        // 清理失败的回复
        if (reply != NULL) {
            freeReplyObject(reply);
        }
    }

    // 连接不可用，释放旧的连接
    if (DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index] != NULL) {
        redisFree(DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index]);
        DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index] = NULL;
    }

    // 尝试重新初始化Redis连接
    DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index] = DBOP_FUN_InitializeRedis(DBOP_VAR_CheckAndReinitializeRedis_config);
    if (DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index] == NULL || DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index]->err) {
        // 如果初始化失败，记录错误并终止服务
        dzlog_error("Failed to reinitialize Redis connection. Stopping service.");
        exit(EXIT_FAILURE);
    }

    return true;
}


// 销毁Redis连接池
void DBOP_FUN_DestroyRedisPool(RedisPool* pool) {
    if (pool == NULL) {
        return;
    }
    
    // 加锁保护Redis连接池销毁
    pthread_mutex_lock(&DBOP_GLV_redisConnection_mutex);
    
    // 释放所有Redis连接
    for (int i = 0; i < pool->DBOP_GLV_poolSize; i++) {
        if (pool->DBOP_GLV_redisConnections[i] != NULL) {
            redisFree(pool->DBOP_GLV_redisConnections[i]);
            pool->DBOP_GLV_redisConnections[i] = NULL;
        }
    }
    
    // 释放连接数组和池结构
    free(pool->DBOP_GLV_redisConnections);
    free(pool);
    
    pthread_mutex_unlock(&DBOP_GLV_redisConnection_mutex);
    
    dzlog_info("Redis connection pool destroyed");
}


// ------------------------redis操作逻辑结束----------------------------


// ------------------------鉴权逻辑开始----------------------------


bool DBOP_FUN_AuthenticateRequest(const AppConfig* DBOP_VAR_AuthenticateRequest_appConfig, const char* DBOP_VAR_AuthenticateRequest_serviceName, const char* DBOP_VAR_AuthenticateRequest_authToken) {
    bool DBOP_VAR_AuthenticateRequest_isAuthenticated = false;
    
    // 线程安全的随机数生成
    unsigned int seed = (unsigned int)pthread_self();
    int DBOP_VAR_AuthenticateRequest_index = rand_r(&seed) % DBOP_GLV_actualRedisPoolSize;
    
    // 确保索引在有效范围内
    if (DBOP_VAR_AuthenticateRequest_index < 0 || DBOP_VAR_AuthenticateRequest_index >= DBOP_GLV_actualRedisPoolSize) {
        dzlog_warn("Redis connection pool index %d out of range (0-%d), using index 0", 
                  DBOP_VAR_AuthenticateRequest_index, DBOP_GLV_actualRedisPoolSize - 1);
        DBOP_VAR_AuthenticateRequest_index = 0;
    }
    
    // 加锁保护Redis连接池访问
    pthread_mutex_lock(&DBOP_GLV_redisConnection_mutex);
    
    if (DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_AuthenticateRequest_index] == NULL || DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_AuthenticateRequest_index]->err) {
        // 如果连接无效，调用DBOP_FUN_CheckAndReinitializeRedis来尝试恢复连接
        if (!DBOP_FUN_CheckAndReinitializeRedis(DBOP_VAR_AuthenticateRequest_appConfig, DBOP_VAR_AuthenticateRequest_index)) {
            // 如果连接仍然无法恢复，处理错误
            dzlog_error("Redis operation failed: Redis connection cannot be established.\n");
            pthread_mutex_unlock(&DBOP_GLV_redisConnection_mutex);
            return DBOP_VAR_AuthenticateRequest_isAuthenticated;
        }
    }
    
    redisReply *DBOP_VAR_AuthenticateRequest_reply = redisCommand(DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_AuthenticateRequest_index], "GET %s", DBOP_VAR_AuthenticateRequest_serviceName);
    
    if (DBOP_VAR_AuthenticateRequest_reply != NULL && DBOP_VAR_AuthenticateRequest_reply->type == REDIS_REPLY_STRING) {
        DBOP_VAR_AuthenticateRequest_isAuthenticated = (strcmp(DBOP_VAR_AuthenticateRequest_reply->str, DBOP_VAR_AuthenticateRequest_authToken) == 0);
    }
    
    freeReplyObject(DBOP_VAR_AuthenticateRequest_reply);
    pthread_mutex_unlock(&DBOP_GLV_redisConnection_mutex);
    
    return DBOP_VAR_AuthenticateRequest_isAuthenticated;
}


// ------------------------鉴权逻辑结束----------------------------


// ------------------------mysql通用操作逻辑开始----------------------------


// mysql声明连接池
DB_CONNECTION *DBOP_GLV_mysqlConnectPool[MAX_DB_POOL_SIZE];


void DBOP_FUN_InitializeMySQLConnection(DB_CONNECTION *DBOP_VAR_InitializeMySQLConnection_connect, AppConfig *DBOP_VAR_InitializeMySQLConnection_cfg) {
    DBOP_VAR_InitializeMySQLConnection_connect->mysql = mysql_init(NULL);
    if (DBOP_VAR_InitializeMySQLConnection_connect->mysql == NULL) {
        dzlog_error("mysql_init() failed.");
        exit(EXIT_FAILURE);
    }

    if (mysql_real_connect(DBOP_VAR_InitializeMySQLConnection_connect->mysql, DBOP_VAR_InitializeMySQLConnection_cfg->DBOP_GLV_dbIp, DBOP_VAR_InitializeMySQLConnection_cfg->DBOP_GLV_dbPrivilegeUser,
                           DBOP_VAR_InitializeMySQLConnection_cfg->DBOP_GLV_dbPrivilegePassword, DBOP_VAR_InitializeMySQLConnection_cfg->DBOP_GLV_dbName,
                           atoi(DBOP_VAR_InitializeMySQLConnection_cfg->DBOP_GLV_dbServerPort), NULL, 0) == NULL) {
        dzlog_error("mysql_real_connect() failed.");
        mysql_close(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
        exit(EXIT_FAILURE);
    }

    // 初始化预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_check_phone = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *check_phone_sql = "SELECT EXISTS(SELECT 1 FROM user WHERE phone_number = ?);";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_check_phone, check_phone_sql, strlen(check_phone_sql))) {
        dzlog_error("Failed to prepare check statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_check_phone));
        exit(EXIT_FAILURE);
    }

    DBOP_VAR_InitializeMySQLConnection_connect->stmt_add_user = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *add_user_sql = "INSERT INTO user (user_id, user_name, user_password, user_permission, phone_number) VALUES (?, ?, ?, ?, ?);";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_add_user, add_user_sql, strlen(add_user_sql))) {
        dzlog_error("Failed to prepare insert statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_add_user));
        exit(EXIT_FAILURE);
    }
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_get_service_passwd = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *stmt_get_service_passwd_sql = "SELECT service_passwd FROM service WHERE service_name = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_get_service_passwd, stmt_get_service_passwd_sql, strlen(stmt_get_service_passwd_sql))) {
        dzlog_error("Failed to prepare check statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_get_service_passwd));
        exit(EXIT_FAILURE);
    }
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_add_project = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *insert_project_sql = "INSERT INTO project (project_id, user_id, user_name, user_age, real_name, project_status, project_create_time) VALUES (?, ?, ?, ?, ?, 1, NOW());";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_add_project, insert_project_sql, strlen(insert_project_sql))) {
        dzlog_error("Failed to prepare insert statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_add_project));
        exit(EXIT_FAILURE);
    }
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_login_time = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_login_time_sql = "UPDATE user SET login_time = NOW() WHERE user_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_login_time, update_login_time_sql, strlen(update_login_time_sql))) {
        dzlog_error("Failed to prepare update statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_login_time));
        exit(EXIT_FAILURE);
    }

    // 初始化更新用户个性化信息预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_info = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_user_info_sql = "UPDATE user SET user_name = ?, avatar = ?, personal_note = ? WHERE user_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_info, update_user_info_sql, strlen(update_user_info_sql))) {
        dzlog_error("Failed to prepare update user info statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_info));
        exit(EXIT_FAILURE);
    }

    // 初始化更新用户手机号预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_phone = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_user_phone_sql = "UPDATE user SET phone_number = ? WHERE user_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_phone, update_user_phone_sql, strlen(update_user_phone_sql))) {
        dzlog_error("Failed to prepare update user phone statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_phone));
        exit(EXIT_FAILURE);
    }

    // 初始化更新用户微信ID预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_weixinid = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_user_weixinid_sql = "UPDATE user SET weixin_openid = ? WHERE user_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_weixinid, update_user_weixinid_sql, strlen(update_user_weixinid_sql))) {
        dzlog_error("Failed to prepare update user weixinid statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_weixinid));
        exit(EXIT_FAILURE);
    }

    // 初始化更新用户权限组预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_permission = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_user_permission_sql = "UPDATE user SET user_permission = ? WHERE user_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_permission, update_user_permission_sql, strlen(update_user_permission_sql))) {
        dzlog_error("Failed to prepare update user permission statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_permission));
        exit(EXIT_FAILURE);
    }

    // 初始化修改用户密码预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_passwd = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_user_passwd_sql = "UPDATE user SET user_password = ? WHERE user_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_passwd, update_user_passwd_sql, strlen(update_user_passwd_sql))) {
        dzlog_error("Failed to prepare update user passwd statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_passwd));
        exit(EXIT_FAILURE);
    }

    // 初始化更新用户项目信息预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_project_info = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_user_project_info_sql = "UPDATE user SET existing_projects = 1, project_id = ? WHERE user_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_project_info, update_user_project_info_sql, strlen(update_user_project_info_sql))) {
        dzlog_error("Failed to prepare update user project info statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_user_project_info));
        exit(EXIT_FAILURE);
    }

    // 初始化更新验证记录预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_verification_record = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_verification_record_sql = "UPDATE project SET verification_record = ? WHERE project_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_verification_record, update_verification_record_sql, strlen(update_verification_record_sql))) {
        dzlog_error("Failed to prepare update verification record statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_verification_record));
        exit(EXIT_FAILURE);
    }

    // 初始化更新项目状态预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_status = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_project_status_sql = "UPDATE project SET project_status = ? WHERE project_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_status, update_project_status_sql, strlen(update_project_status_sql))) {
        dzlog_error("Failed to prepare update project status statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_status));
        exit(EXIT_FAILURE);
    }

    // 初始化更新病情记录预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_pathography = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_project_pathography_sql = "UPDATE project SET pathography = ? WHERE project_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_pathography, update_project_pathography_sql, strlen(update_project_pathography_sql))) {
        dzlog_error("Failed to prepare update project pathography statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_pathography));
        exit(EXIT_FAILURE);
    }

    // 初始化更新项目负责志愿者ID预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_volunteer = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *update_project_volunteer_sql = "UPDATE project SET current_volunteer = ?, previous_volunteers = ? WHERE project_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_volunteer, update_project_volunteer_sql, strlen(update_project_volunteer_sql))) {
        dzlog_error("Failed to prepare update project volunteer statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_update_project_volunteer));
        exit(EXIT_FAILURE);
    }

    // 初始化查询项目志愿者信息预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_get_project_volunteer_info = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *get_project_volunteer_info_sql = "SELECT current_volunteer, previous_volunteers FROM project WHERE project_id = ?;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_get_project_volunteer_info, get_project_volunteer_info_sql, strlen(get_project_volunteer_info_sql))) {
        dzlog_error("Failed to prepare get project volunteer info statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_get_project_volunteer_info));
        exit(EXIT_FAILURE);
    }
    // 初始化新增捐款记录预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_make_donation = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *make_donation_sql = "INSERT INTO donation_ledger (ledger_id, project_id, user_id, sufferer_user_id, donor_user_name, sufferer_real_name, sufferer_user_name, amount, transaction_time, note, payment_method, method_id, transaction_type, status, change_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, ?, 0, 1, NOW());";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_make_donation, make_donation_sql, strlen(make_donation_sql))) {
        dzlog_error("Failed to prepare make donation statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_make_donation));
        exit(EXIT_FAILURE);
    }
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_failed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_payment_failed_sql = "UPDATE donation_ledger SET status = 2, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 0;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_failed, set_status_payment_failed_sql, strlen(set_status_payment_failed_sql))) {
        dzlog_error("Failed to prepare mark payment failed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_failed));
        exit(EXIT_FAILURE);
    }

    // 初始化标记处理中预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_processing = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_payment_processing_sql = "UPDATE donation_ledger SET status = 3, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 0;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_processing, set_status_payment_processing_sql, strlen(set_status_payment_processing_sql))) {
        dzlog_error("Failed to prepare mark payment processing statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_processing));
        exit(EXIT_FAILURE);
    }

    // 初始化标记处理失败预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_process_failed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_payment_process_failed_sql = "UPDATE donation_ledger SET status = 4, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 0;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_process_failed, set_status_payment_process_failed_sql, strlen(set_status_payment_process_failed_sql))) {
        dzlog_error("Failed to prepare mark payment process failed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_process_failed));
        exit(EXIT_FAILURE);
    }

    // 初始化标记退款中预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refunding = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_payment_refunding_sql = "UPDATE donation_ledger SET status = 5, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 0;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refunding, set_status_payment_refunding_sql, strlen(set_status_payment_refunding_sql))) {
        dzlog_error("Failed to prepare mark payment refunding statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refunding));
        exit(EXIT_FAILURE);
    }

    // 初始化标记退款失败预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refund_failed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_payment_refund_failed_sql = "UPDATE donation_ledger SET status = 6, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 0;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refund_failed, set_status_payment_refund_failed_sql, strlen(set_status_payment_refund_failed_sql))) {
        dzlog_error("Failed to prepare mark payment refund failed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refund_failed));
        exit(EXIT_FAILURE);
    }

    // 初始化标记退款完成预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refund_completed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_payment_refund_completed_sql = "UPDATE donation_ledger SET status = 7, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 0;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refund_completed, set_status_payment_refund_completed_sql, strlen(set_status_payment_refund_completed_sql))) {
        dzlog_error("Failed to prepare mark payment refund completed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_refund_completed));
        exit(EXIT_FAILURE);
    }

    // 初始化标记完成预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_completed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_payment_completed_sql = "UPDATE donation_ledger SET status = 0, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 0;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_completed, set_status_payment_completed_sql, strlen(set_status_payment_completed_sql))) {
        dzlog_error("Failed to prepare mark payment completed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_payment_completed));
        exit(EXIT_FAILURE);
    }

    // 初始化新增退款待选择记录预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_refund_pending_selection = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *create_refund_pending_selection_sql = "INSERT INTO donation_ledger (ledger_id, project_id, user_id, sufferer_user_id, donor_user_name, sufferer_real_name, sufferer_user_name, amount, transaction_time, note, target_user_id, transaction_type, status, change_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, 1, 1, NOW());";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_refund_pending_selection, create_refund_pending_selection_sql, strlen(create_refund_pending_selection_sql))) {
        dzlog_error("Failed to prepare create refund pending selection statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_refund_pending_selection));
        exit(EXIT_FAILURE);
    }

    // 初始化标记退款待选择转为退款类型预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_type_refund = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_type_refund_sql = "UPDATE donation_ledger SET transaction_type = 2, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 1;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_type_refund, set_type_refund_sql, strlen(set_type_refund_sql))) {
        dzlog_error("Failed to prepare mark refund selected statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_type_refund));
        exit(EXIT_FAILURE);
    }

    // 初始化标记退款失败预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_refund_failed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_refund_failed_sql = "UPDATE donation_ledger SET status = 6, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 2;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_refund_failed, set_status_refund_failed_sql, strlen(set_status_refund_failed_sql))) {
        dzlog_error("Failed to prepare mark refund failed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_refund_failed));
        exit(EXIT_FAILURE);
    }

    // 初始化标记退款完成预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_refund_completed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_refund_completed_sql = "UPDATE donation_ledger SET status = 7, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 2;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_refund_completed, set_status_refund_completed_sql, strlen(set_status_refund_completed_sql))) {
        dzlog_error("Failed to prepare mark refund completed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_refund_completed));
        exit(EXIT_FAILURE);
    }

    // 初始化标记退款待选择转为转应急池类型预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_type_emergency_pool = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_type_emergency_pool_sql = "UPDATE donation_ledger SET transaction_type = 3, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 1;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_type_emergency_pool, set_type_emergency_pool_sql, strlen(set_type_emergency_pool_sql))) {
        dzlog_error("Failed to prepare mark emergency pool statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_type_emergency_pool));
        exit(EXIT_FAILURE);
    }

    // 初始化新增患者使用记录预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_patient_usage = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *create_patient_usage_sql = "INSERT INTO donation_ledger (ledger_id, project_id, user_id, sufferer_user_id, sufferer_real_name, sufferer_user_name, amount, transaction_time, note, payment_method, transaction_type, status, volunteer_audit_id, change_time) VALUES (?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, 4, 3, ?, NOW());";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_patient_usage, create_patient_usage_sql, strlen(create_patient_usage_sql))) {
        dzlog_error("Failed to prepare create patient usage statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_patient_usage));
        exit(EXIT_FAILURE);
    }

    // 初始化新增志愿者审计记录预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_volunteer_audit = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *create_volunteer_audit_sql = "INSERT INTO donation_volunteer_audit (audit_id, verification_record, last_change_time) VALUES (?, ?, NOW());";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_volunteer_audit, create_volunteer_audit_sql, strlen(create_volunteer_audit_sql))) {
        dzlog_error("Failed to prepare create volunteer audit statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_volunteer_audit));
        exit(EXIT_FAILURE);
    }

    // 初始化患者使用记录状态更新预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_process_failed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_patient_usage_process_failed_sql = "UPDATE donation_ledger SET status = 4, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 4;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_process_failed, set_status_patient_usage_process_failed_sql, strlen(set_status_patient_usage_process_failed_sql))) {
        dzlog_error("Failed to prepare set patient usage process failed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_process_failed));
        exit(EXIT_FAILURE);
    }

    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_paying = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_patient_usage_paying_sql = "UPDATE donation_ledger SET status = 8, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 4;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_paying, set_status_patient_usage_paying_sql, strlen(set_status_patient_usage_paying_sql))) {
        dzlog_error("Failed to prepare set patient usage paying statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_paying));
        exit(EXIT_FAILURE);
    }

    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_pay_failed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_patient_usage_pay_failed_sql = "UPDATE donation_ledger SET status = 9, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 4;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_pay_failed, set_status_patient_usage_pay_failed_sql, strlen(set_status_patient_usage_pay_failed_sql))) {
        dzlog_error("Failed to prepare set patient usage pay failed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_pay_failed));
        exit(EXIT_FAILURE);
    }

    DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_completed = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *set_status_patient_usage_completed_sql = "UPDATE donation_ledger SET status = 0, change_time = NOW() WHERE ledger_id = ? AND transaction_type = 4;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_completed, set_status_patient_usage_completed_sql, strlen(set_status_patient_usage_completed_sql))) {
        dzlog_error("Failed to prepare set patient usage completed statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_set_status_patient_usage_completed));
        exit(EXIT_FAILURE);
    }

    // 初始化新增应急使用记录预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_emergency_usage = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *create_emergency_usage_sql = "INSERT INTO donation_ledger (ledger_id, project_id, user_id, sufferer_user_id, sufferer_real_name, sufferer_user_name, amount, transaction_time, note, payment_method, transaction_type, status, volunteer_audit_id) VALUES (?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, 5, 3, ?);";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_emergency_usage, create_emergency_usage_sql, strlen(create_emergency_usage_sql))) {
        dzlog_error("Failed to prepare create emergency usage statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_create_emergency_usage));
        exit(EXIT_FAILURE);
    }

    // 初始化检查待核销应急使用记录预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_check_pending_emergency_usage = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *check_pending_emergency_usage_sql = "SELECT COUNT(*) FROM donation_ledger WHERE project_id = ? AND sufferer_user_id = ? AND transaction_type = 5 AND status != 14;";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_check_pending_emergency_usage, check_pending_emergency_usage_sql, strlen(check_pending_emergency_usage_sql))) {
        dzlog_error("Failed to prepare check pending emergency usage statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_check_pending_emergency_usage));
        exit(EXIT_FAILURE);
    }
}

// 初始化mysql连接池
void DBOP_FUN_InitializeConnPool(AppConfig *DBOP_VAR_InitializeConnPool_cfg) {
    // 加锁保护MySQL连接池初始化
    pthread_mutex_lock(&DBOP_GLV_mysqlConnection_mutex);
    
    // 从配置获取数据库连接池大小，默认为10
    int dbPoolSize = 10;
    if (DBOP_VAR_InitializeConnPool_cfg->DBOP_GLV_databasePoolSize != NULL) {
        dbPoolSize = atoi(DBOP_VAR_InitializeConnPool_cfg->DBOP_GLV_databasePoolSize);
        if (dbPoolSize <= 0) {
            dbPoolSize = 10; // 如果配置无效，使用默认值
        }
    }
    
    // 确保连接池大小不超过最大值
    if (dbPoolSize > MAX_DB_POOL_SIZE) {
        dbPoolSize = MAX_DB_POOL_SIZE;
        dzlog_warn("Database pool size exceeds maximum limit, using %d instead", MAX_DB_POOL_SIZE);
    }
    
    // 保存实际的连接池大小
    DBOP_GLV_actualDBPoolSize = dbPoolSize;
    
    for (int i = 0; i < dbPoolSize; i++) {
        DBOP_GLV_mysqlConnectPool[i] = malloc(sizeof(DB_CONNECTION));  // 分配内存
        DBOP_FUN_InitializeMySQLConnection(DBOP_GLV_mysqlConnectPool[i], DBOP_VAR_InitializeConnPool_cfg);
    }
    
    pthread_mutex_unlock(&DBOP_GLV_mysqlConnection_mutex);
    dzlog_info("Initialized database connection pool with %d connections", dbPoolSize);
}

// 销毁mysql连接池
void DBOP_FUN_DestroyConnPool() {
    // 加锁保护MySQL连接池销毁
    pthread_mutex_lock(&DBOP_GLV_mysqlConnection_mutex);
    
    for (int i = 0; i < DBOP_GLV_actualDBPoolSize; i++) {
        if (DBOP_GLV_mysqlConnectPool[i] == NULL) {
            continue;  // 跳过未初始化的连接
        }
        
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_check_phone != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_check_phone);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_add_user != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_add_user);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_get_service_passwd != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_get_service_passwd);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_add_project != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_add_project);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_login_time != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_login_time);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_info != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_info);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_phone != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_phone);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_weixinid != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_weixinid);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_permission != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_permission);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_passwd != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_passwd);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_project_info != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_user_project_info);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_verification_record != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_verification_record);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_project_status != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_project_status);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_project_pathography != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_project_pathography);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_update_project_volunteer != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_update_project_volunteer);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_get_project_volunteer_info != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_get_project_volunteer_info);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_make_donation != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_make_donation);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_failed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_failed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_processing != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_processing);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_process_failed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_process_failed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_refunding != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_refunding);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_refund_failed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_refund_failed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_refund_completed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_refund_completed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_completed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_payment_completed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_create_refund_pending_selection != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_create_refund_pending_selection);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_type_refund != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_type_refund);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_refund_failed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_refund_failed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_refund_completed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_refund_completed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_type_emergency_pool != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_type_emergency_pool);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_create_patient_usage != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_create_patient_usage);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_create_volunteer_audit != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_create_volunteer_audit);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_patient_usage_process_failed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_patient_usage_process_failed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_patient_usage_paying != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_patient_usage_paying);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_patient_usage_pay_failed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_patient_usage_pay_failed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_patient_usage_completed != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_set_status_patient_usage_completed);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_create_emergency_usage != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_create_emergency_usage);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_check_pending_emergency_usage != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_check_pending_emergency_usage);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->mysql != NULL) {
            mysql_close(DBOP_GLV_mysqlConnectPool[i]->mysql);
        }
        free(DBOP_GLV_mysqlConnectPool[i]);
    }
    
    pthread_mutex_unlock(&DBOP_GLV_mysqlConnection_mutex);
    dzlog_info("Database connection pool destroyed");
}


// 重新初始化mysql连接
void DBOP_FUN_ReinitializeConnPool(AppConfig *DBOP_VAR_ReinitializeConnPool_cfg, int index) {
    DB_CONNECTION *DBOP_VAR_ReinitializeConnPool_connect = DBOP_GLV_mysqlConnectPool[index];
    
    // 先关闭旧的连接和预处理语句句柄
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_check_phone != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_check_phone);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_add_user != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_add_user);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_get_service_passwd != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_get_service_passwd);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_add_project != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_add_project);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_login_time != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_login_time);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_info != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_info);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_phone != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_phone);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_weixinid != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_weixinid);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_permission != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_permission);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_passwd != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_passwd);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_project_info != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_user_project_info);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_verification_record != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_verification_record);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_project_status != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_project_status);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_project_pathography != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_project_pathography);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_update_project_volunteer != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_update_project_volunteer);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_get_project_volunteer_info != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_get_project_volunteer_info);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_make_donation != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_make_donation);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_failed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_failed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_processing != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_processing);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_process_failed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_process_failed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_refunding != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_refunding);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_refund_failed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_refund_failed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_refund_completed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_refund_completed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_completed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_payment_completed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_create_refund_pending_selection != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_create_refund_pending_selection);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_type_refund != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_type_refund);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_refund_failed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_refund_failed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_refund_completed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_refund_completed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_type_emergency_pool != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_type_emergency_pool);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_create_patient_usage != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_create_patient_usage);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_create_volunteer_audit != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_create_volunteer_audit);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_patient_usage_process_failed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_patient_usage_process_failed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_patient_usage_paying != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_patient_usage_paying);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_patient_usage_pay_failed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_patient_usage_pay_failed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_patient_usage_completed != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_set_status_patient_usage_completed);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_create_emergency_usage != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_create_emergency_usage);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_check_pending_emergency_usage != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_check_pending_emergency_usage);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->mysql != NULL) {
        mysql_close(DBOP_VAR_ReinitializeConnPool_connect->mysql);
    }

    // 初始化新的MySQL连接和预处理语句句柄
    DBOP_FUN_InitializeMySQLConnection(DBOP_VAR_ReinitializeConnPool_connect, DBOP_VAR_ReinitializeConnPool_cfg);

    dzlog_debug("Reinitialized MySQL pool with index: %d", index);
}


// 检查mysql数据库连接状态
DB_CONNECTION* DBOP_FUN_GetConnectFromPool(AppConfig *DBOP_VAR_GetConnectFromPool_cfg, int index) {
    // 确保索引在有效范围内
    if (index < 0 || index >= DBOP_GLV_actualDBPoolSize) {
        dzlog_warn("Connection pool index %d out of range (0-%d), using index 0 instead", 
                  index, DBOP_GLV_actualDBPoolSize - 1);
        index = 0;  // 使用第一个连接作为备选
    }

    // 加锁保护MySQL连接池访问
    pthread_mutex_lock(&DBOP_GLV_mysqlConnection_mutex);
    
    DB_CONNECTION *DBOP_VAR_GetConnectFromPool_connect = DBOP_GLV_mysqlConnectPool[index];
    
    if (mysql_ping(DBOP_VAR_GetConnectFromPool_connect->mysql) != 0) {  // 检查连接是否有效
        dzlog_error("Lost connection to database with index: %d. Reconnecting...", index);
        DBOP_FUN_ReinitializeConnPool(DBOP_VAR_GetConnectFromPool_cfg, index);
    }

    pthread_mutex_unlock(&DBOP_GLV_mysqlConnection_mutex);
    return DBOP_VAR_GetConnectFromPool_connect;
}


// ------------------------mysql通用操作逻辑结束----------------------------


// ------------------------API通用辅助函数开始----------------------------

// 获取或生成请求ID
const char* DBOP_FUN_GetOrGenerateRequestId(struct evhttp_request *DBOP_VAR_GetOrGenerateRequestId_request, char *DBOP_VAR_GetOrGenerateRequestId_uuidBuffer) {
    const char *DBOP_VAR_GetOrGenerateRequestId_requestId = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_GetOrGenerateRequestId_request), "X-Request-ID");
    if (!DBOP_VAR_GetOrGenerateRequestId_requestId) {
        // 如果请求中没有X-Request-ID头部，生成一个UUID作为请求ID
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, DBOP_VAR_GetOrGenerateRequestId_uuidBuffer);
        DBOP_VAR_GetOrGenerateRequestId_requestId = DBOP_VAR_GetOrGenerateRequestId_uuidBuffer;
    }
    return DBOP_VAR_GetOrGenerateRequestId_requestId;
}

// 执行请求鉴权
bool DBOP_FUN_HandleAuthentication(struct evhttp_request *DBOP_VAR_HandleAuthentication_request, AppConfig *DBOP_VAR_HandleAuthentication_cfg, const char *DBOP_VAR_HandleAuthentication_requestId) {
    const char *DBOP_VAR_HandleAuthentication_serviceName = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_HandleAuthentication_request), "ServiceName");
    const char *DBOP_VAR_HandleAuthentication_token = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_HandleAuthentication_request), "Authorization");
    
    if (DBOP_FUN_AuthenticateRequest(DBOP_VAR_HandleAuthentication_cfg, DBOP_VAR_HandleAuthentication_serviceName, DBOP_VAR_HandleAuthentication_token)) {
        dzlog_info("[req: %s] Request authentication approval.", DBOP_VAR_HandleAuthentication_requestId);
        return true;
    } else {
        dzlog_warn("[req: %s] Unauthorized access attempt.", DBOP_VAR_HandleAuthentication_requestId);
        evhttp_send_reply(DBOP_VAR_HandleAuthentication_request, 401, "Unauthorized", NULL);
        return false;
    }
}

// 解析POST数据为JSON
json_t* DBOP_FUN_ParsePostDataToJson(struct evhttp_request *DBOP_VAR_ParsePostDataToJson_request, const char *DBOP_VAR_ParsePostDataToJson_requestId) {
    struct evbuffer *DBOP_VAR_ParsePostDataToJson_inputBuffer = evhttp_request_get_input_buffer(DBOP_VAR_ParsePostDataToJson_request);
    size_t DBOP_VAR_ParsePostDataToJson_bufferLen = evbuffer_get_length(DBOP_VAR_ParsePostDataToJson_inputBuffer);
    char DBOP_VAR_ParsePostDataToJson_postData[DBOP_VAR_ParsePostDataToJson_bufferLen + 1];
    evbuffer_remove(DBOP_VAR_ParsePostDataToJson_inputBuffer, DBOP_VAR_ParsePostDataToJson_postData, DBOP_VAR_ParsePostDataToJson_bufferLen);
    DBOP_VAR_ParsePostDataToJson_postData[DBOP_VAR_ParsePostDataToJson_bufferLen] = '\0';

    json_error_t DBOP_VAR_ParsePostDataToJson_dataJsonError;
    json_t *DBOP_VAR_ParsePostDataToJson_dataJsonAll = json_loads(DBOP_VAR_ParsePostDataToJson_postData, 0, &DBOP_VAR_ParsePostDataToJson_dataJsonError);
    dzlog_info("[req: %s] Parsing POST data.", DBOP_VAR_ParsePostDataToJson_requestId);
    
    if (!DBOP_VAR_ParsePostDataToJson_dataJsonAll) {
        dzlog_error("[req: %s] Failed to parse JSON from request body.", DBOP_VAR_ParsePostDataToJson_requestId);
        evhttp_send_reply(DBOP_VAR_ParsePostDataToJson_request, 400, "Bad Request", NULL);
        return NULL;
    }
    
    return DBOP_VAR_ParsePostDataToJson_dataJsonAll;
}

// 获取数据库连接
DB_CONNECTION* DBOP_FUN_GetDatabaseConnection(AppConfig *DBOP_VAR_GetDatabaseConnection_cfg) {
    // 使用线程安全的随机数生成
    unsigned int seed = (unsigned int)pthread_self();
    int index = rand_r(&seed) % DBOP_GLV_actualDBPoolSize;
    return DBOP_FUN_GetConnectFromPool(DBOP_VAR_GetDatabaseConnection_cfg, index);
}

// 发送标准HTTP响应
void DBOP_FUN_SendStandardResponse(struct evhttp_request *DBOP_VAR_SendStandardResponse_request, int DBOP_VAR_SendStandardResponse_result, const char *DBOP_VAR_SendStandardResponse_requestId, const char *DBOP_VAR_SendStandardResponse_operationName) {
    if (DBOP_VAR_SendStandardResponse_result == 0) {
        evhttp_send_reply(DBOP_VAR_SendStandardResponse_request, HTTP_OK, "OK", NULL);
        dzlog_info("[req: %s] Successfully completed %s, returning 200", DBOP_VAR_SendStandardResponse_requestId, DBOP_VAR_SendStandardResponse_operationName);
    } else if (DBOP_VAR_SendStandardResponse_result == 1) {
        evhttp_send_reply(DBOP_VAR_SendStandardResponse_request, 400, "Bad Request", NULL);
        dzlog_warn("[req: %s] %s failed with validation error, returning 400", DBOP_VAR_SendStandardResponse_requestId, DBOP_VAR_SendStandardResponse_operationName);
    } else {
        evhttp_send_reply(DBOP_VAR_SendStandardResponse_request, HTTP_INTERNAL, "Internal Server Error", NULL);
        dzlog_error("[req: %s] Failed to complete %s, returning 500", DBOP_VAR_SendStandardResponse_requestId, DBOP_VAR_SendStandardResponse_operationName);
    }
}

// ------------------------API通用辅助函数结束----------------------------



// ------------------------mysql添加用户api逻辑开始----------------------------


// 添加用户的sql数据化输出
int DBOP_FUN_ExecuteAddUser(DB_CONNECTION *DBOP_VAR_ExecuteAddUser_connect, const char *DBOP_VAR_ExecuteAddUser_userId, const char *DBOP_VAR_ExecuteAddUser_userName, const char *DBOP_VAR_ExecuteAddUser_userPasswd, int DBOP_VAR_ExecuteAddUser_userPermission, const char *DBOP_VAR_ExecuteAddUser_phoneNumber, const char *DBOP_VAR_ExecuteAddUser_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteAddUser is starting", DBOP_VAR_ExecuteAddUser_requestId);

    // ------------------------手机号检查逻辑开始----------------------------
    dzlog_info("[req: %s] DBOP_FUN_ExecuteAddUser is checking", DBOP_VAR_ExecuteAddUser_requestId);

    char DBOP_VAR_ExecuteAddUser_noConstWaitAddUserPhone[256];
    strncpy(DBOP_VAR_ExecuteAddUser_noConstWaitAddUserPhone, DBOP_VAR_ExecuteAddUser_phoneNumber, sizeof(DBOP_VAR_ExecuteAddUser_noConstWaitAddUserPhone) - 1);
    DBOP_VAR_ExecuteAddUser_noConstWaitAddUserPhone[sizeof(DBOP_VAR_ExecuteAddUser_noConstWaitAddUserPhone) - 1] = '\0';

    // 检查phone是否存在
    MYSQL_BIND DBOP_VAR_ExecuteAddUser_checkBind[1];
    memset(DBOP_VAR_ExecuteAddUser_checkBind, 0, sizeof(DBOP_VAR_ExecuteAddUser_checkBind));

    DBOP_VAR_ExecuteAddUser_checkBind[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUser_checkBind[0].buffer = (char *)DBOP_VAR_ExecuteAddUser_noConstWaitAddUserPhone;
    DBOP_VAR_ExecuteAddUser_checkBind[0].buffer_length = strlen(DBOP_VAR_ExecuteAddUser_noConstWaitAddUserPhone);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone, DBOP_VAR_ExecuteAddUser_checkBind)) {
        dzlog_error("[req: %s] Failed to bind check param: %s", DBOP_VAR_ExecuteAddUser_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone));
        return -1; // 表示绑定参数失败
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone)) {
        dzlog_error("[req: %s] Failed to execute check statement: %s", DBOP_VAR_ExecuteAddUser_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone));
        return -1; // 表示执行语句失败
    }

    MYSQL_BIND DBOP_VAR_ExecuteAddUser_checkResult[1];
    memset(DBOP_VAR_ExecuteAddUser_checkResult, 0, sizeof(DBOP_VAR_ExecuteAddUser_checkResult));
    int count;
    DBOP_VAR_ExecuteAddUser_checkResult[0].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteAddUser_checkResult[0].buffer = &count;

    if (mysql_stmt_bind_result(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone, DBOP_VAR_ExecuteAddUser_checkResult)) {
        dzlog_error("[req: %s] Failed to bind check result: %s", DBOP_VAR_ExecuteAddUser_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone));
        return -1; // 表示绑定结果失败
    }

    if (mysql_stmt_fetch(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone)) {
        dzlog_error("[req: %s] Failed to fetch check result: %s", DBOP_VAR_ExecuteAddUser_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone));
        return -1; // 表示获取结果失败
    }
    mysql_stmt_free_result(DBOP_VAR_ExecuteAddUser_connect->stmt_check_phone);

    if (count > 0) {
        dzlog_info("[req: %s] Phone number already exists", DBOP_VAR_ExecuteAddUser_requestId);
        return 1; // 表示phone已存在
    }
    // ------------------------手机号检查逻辑结束----------------------------

    MYSQL_BIND DBOP_VAR_ExecuteAddUser_bindParams[5];
    memset(DBOP_VAR_ExecuteAddUser_bindParams, 0, sizeof(DBOP_VAR_ExecuteAddUser_bindParams));

    DBOP_VAR_ExecuteAddUser_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUser_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteAddUser_userId;
    DBOP_VAR_ExecuteAddUser_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteAddUser_userId);

    DBOP_VAR_ExecuteAddUser_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUser_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteAddUser_userName;
    DBOP_VAR_ExecuteAddUser_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteAddUser_userName);

    DBOP_VAR_ExecuteAddUser_bindParams[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUser_bindParams[2].buffer = (char *)DBOP_VAR_ExecuteAddUser_userPasswd;
    DBOP_VAR_ExecuteAddUser_bindParams[2].buffer_length = strlen(DBOP_VAR_ExecuteAddUser_userPasswd);

    DBOP_VAR_ExecuteAddUser_bindParams[3].buffer_type = MYSQL_TYPE_TINY;
    DBOP_VAR_ExecuteAddUser_bindParams[3].buffer = &DBOP_VAR_ExecuteAddUser_userPermission;
    DBOP_VAR_ExecuteAddUser_bindParams[3].buffer_length = sizeof(DBOP_VAR_ExecuteAddUser_userPermission);

    DBOP_VAR_ExecuteAddUser_bindParams[4].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUser_bindParams[4].buffer = (char *)DBOP_VAR_ExecuteAddUser_phoneNumber;
    DBOP_VAR_ExecuteAddUser_bindParams[4].buffer_length = strlen(DBOP_VAR_ExecuteAddUser_phoneNumber);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteAddUser_connect->stmt_add_user, DBOP_VAR_ExecuteAddUser_bindParams)) {
        dzlog_error("[req: %s] Failed to bind insert param: %s", DBOP_VAR_ExecuteAddUser_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUser_connect->stmt_add_user));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteAddUser_connect->stmt_add_user)) {
        dzlog_error("[req: %s] Failed to execute insert statement: %s", DBOP_VAR_ExecuteAddUser_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUser_connect->stmt_add_user));
        return -1;
    }

    dzlog_info("[req: %s] Successfully executed insert for user", DBOP_VAR_ExecuteAddUser_requestId);
    return 0;
}

void DBOP_FUN_ApiAddUser(struct evhttp_request *DBOP_VAR_ApiAddUser_request, void *DBOP_VAR_ApiAddUser_voidCfg) {
    AppConfig *DBOP_VAR_ApiAddUser_cfg = (AppConfig *)DBOP_VAR_ApiAddUser_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiAddUser_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiAddUser_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiAddUser.", DBOP_VAR_ApiAddUser_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiAddUser_request, DBOP_VAR_ApiAddUser_cfg, DBOP_VAR_ApiAddUser_requestId)) {
        return;
    }

    // 解析POST数据
    json_t *DBOP_VAR_ApiAddUser_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiAddUser_request, DBOP_VAR_ApiAddUser_requestId);
    if (!DBOP_VAR_ApiAddUser_dataJsonAll) {
        return;
    }

    json_t *DBOP_VAR_ApiAddUser_dataJsonUserId = json_object_get(DBOP_VAR_ApiAddUser_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiAddUser_dataJsonUserName = json_object_get(DBOP_VAR_ApiAddUser_dataJsonAll, "user_name");
    json_t *DBOP_VAR_ApiAddUser_dataJsonUserPasswd = json_object_get(DBOP_VAR_ApiAddUser_dataJsonAll, "user_passwd");
    json_t *DBOP_VAR_ApiAddUser_dataJsonUserPermission = json_object_get(DBOP_VAR_ApiAddUser_dataJsonAll, "user_permission");
    json_t *DBOP_VAR_ApiAddUser_dataJsonPhoneNumber = json_object_get(DBOP_VAR_ApiAddUser_dataJsonAll, "phone_number");

    if (!json_is_string(DBOP_VAR_ApiAddUser_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiAddUser_dataJsonUserName) ||
        !json_is_string(DBOP_VAR_ApiAddUser_dataJsonUserPasswd) ||
        !json_is_integer(DBOP_VAR_ApiAddUser_dataJsonUserPermission) ||
        !json_is_string(DBOP_VAR_ApiAddUser_dataJsonPhoneNumber)) {
        dzlog_error("[req: %s] Invalid JSON data received.", DBOP_VAR_ApiAddUser_requestId);
        evhttp_send_reply(DBOP_VAR_ApiAddUser_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiAddUser_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiAddUser_userId = json_string_value(DBOP_VAR_ApiAddUser_dataJsonUserId);
    const char *DBOP_VAR_ApiAddUser_userName = json_string_value(DBOP_VAR_ApiAddUser_dataJsonUserName);
    const char *DBOP_VAR_ApiAddUser_userPasswd = json_string_value(DBOP_VAR_ApiAddUser_dataJsonUserPasswd);
    int DBOP_VAR_ApiAddUser_userPermission = json_integer_value(DBOP_VAR_ApiAddUser_dataJsonUserPermission);
    const char *DBOP_VAR_ApiAddUser_phoneNumber = json_string_value(DBOP_VAR_ApiAddUser_dataJsonPhoneNumber);
    
    dzlog_info("[%s] Executing database operation for ApiAddUser: userId=%s, userName=%s, permissionLevel=%d, phoneNumber=%s", DBOP_VAR_ApiAddUser_requestId, DBOP_VAR_ApiAddUser_userId, DBOP_VAR_ApiAddUser_userName, DBOP_VAR_ApiAddUser_userPermission, DBOP_VAR_ApiAddUser_phoneNumber);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiAddUser_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiAddUser_cfg);
    int result = DBOP_FUN_ExecuteAddUser(DBOP_VAR_ApiAddUser_mysqlConnect, DBOP_VAR_ApiAddUser_userId, DBOP_VAR_ApiAddUser_userName, DBOP_VAR_ApiAddUser_userPasswd, DBOP_VAR_ApiAddUser_userPermission, DBOP_VAR_ApiAddUser_phoneNumber, DBOP_VAR_ApiAddUser_requestId);

    // 发送响应
    if (result == 0) {
        evhttp_send_reply(DBOP_VAR_ApiAddUser_request, HTTP_OK, "OK", NULL);
        dzlog_info("[req: %s] Successfully created user, returning 200", DBOP_VAR_ApiAddUser_requestId);
    } else if (result == 1) {
        evhttp_send_reply(DBOP_VAR_ApiAddUser_request, 400, "Phone number already exists", NULL);
        dzlog_error("[req: %s] Phone number already exists, returning 400", DBOP_VAR_ApiAddUser_requestId);
    } else {
        evhttp_send_reply(DBOP_VAR_ApiAddUser_request, HTTP_INTERNAL, "Internal Server Error", NULL);
        dzlog_error("[req: %s] Failed to create user, returning 500", DBOP_VAR_ApiAddUser_requestId);
    }

    json_decref(DBOP_VAR_ApiAddUser_dataJsonAll);
}
// ------------------------mysql添加用户api逻辑结束----------------------------


// ------------------------mysql创建project api逻辑开始----------------------------

// 创建项目的sql数据化输出
int DBOP_FUN_ExecuteCreateProject(DB_CONNECTION *DBOP_VAR_ExecuteCreateProject_connect, const char *DBOP_VAR_ExecuteCreateProject_projectId, const char *DBOP_VAR_ExecuteCreateProject_userID, const char *DBOP_VAR_ExecuteCreateProject_userName, int DBOP_VAR_ExecuteCreateProject_userAge, const char *DBOP_VAR_ExecuteCreateProject_realName, const char *DBOP_VAR_ExecuteCreateProject_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteCreateProject is starting", DBOP_VAR_ExecuteCreateProject_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteCreateProject_noConstProjectId[256];
    char DBOP_VAR_ExecuteCreateProject_noConstUserId[256];
    char DBOP_VAR_ExecuteCreateProject_noConstUserName[256];
    char DBOP_VAR_ExecuteCreateProject_noConstRealName[256];

    strncpy(DBOP_VAR_ExecuteCreateProject_noConstProjectId, DBOP_VAR_ExecuteCreateProject_projectId, sizeof(DBOP_VAR_ExecuteCreateProject_noConstProjectId) - 1);
    DBOP_VAR_ExecuteCreateProject_noConstProjectId[sizeof(DBOP_VAR_ExecuteCreateProject_noConstProjectId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateProject_noConstUserId, DBOP_VAR_ExecuteCreateProject_userID, sizeof(DBOP_VAR_ExecuteCreateProject_noConstUserId) - 1);
    DBOP_VAR_ExecuteCreateProject_noConstUserId[sizeof(DBOP_VAR_ExecuteCreateProject_noConstUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateProject_noConstUserName, DBOP_VAR_ExecuteCreateProject_userName, sizeof(DBOP_VAR_ExecuteCreateProject_noConstUserName) - 1);
    DBOP_VAR_ExecuteCreateProject_noConstUserName[sizeof(DBOP_VAR_ExecuteCreateProject_noConstUserName) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateProject_noConstRealName, DBOP_VAR_ExecuteCreateProject_realName, sizeof(DBOP_VAR_ExecuteCreateProject_noConstRealName) - 1);
    DBOP_VAR_ExecuteCreateProject_noConstRealName[sizeof(DBOP_VAR_ExecuteCreateProject_noConstRealName) - 1] = '\0';

    // 开始事务
    if (mysql_autocommit(DBOP_VAR_ExecuteCreateProject_connect->mysql, 0)) {
        dzlog_error("[req: %s] Failed to start transaction: %s", DBOP_VAR_ExecuteCreateProject_requestId, mysql_error(DBOP_VAR_ExecuteCreateProject_connect->mysql));
        return -1;
    }

    // 第一步：插入项目记录
    MYSQL_BIND DBOP_VAR_ExecuteCreateProject_bindParams[5];
    memset(DBOP_VAR_ExecuteCreateProject_bindParams, 0, sizeof(DBOP_VAR_ExecuteCreateProject_bindParams));

    // 绑定project_id
    DBOP_VAR_ExecuteCreateProject_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteCreateProject_noConstProjectId;
    DBOP_VAR_ExecuteCreateProject_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_noConstProjectId);

    // 绑定user_id
    DBOP_VAR_ExecuteCreateProject_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteCreateProject_noConstUserId;
    DBOP_VAR_ExecuteCreateProject_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_noConstUserId);

    // 绑定user_name
    DBOP_VAR_ExecuteCreateProject_bindParams[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindParams[2].buffer = (char *)DBOP_VAR_ExecuteCreateProject_noConstUserName;
    DBOP_VAR_ExecuteCreateProject_bindParams[2].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_noConstUserName);

    // 绑定user_age
    DBOP_VAR_ExecuteCreateProject_bindParams[3].buffer_type = MYSQL_TYPE_TINY;
    DBOP_VAR_ExecuteCreateProject_bindParams[3].buffer = &DBOP_VAR_ExecuteCreateProject_userAge;
    DBOP_VAR_ExecuteCreateProject_bindParams[3].buffer_length = sizeof(DBOP_VAR_ExecuteCreateProject_userAge);

    // 绑定real_name
    DBOP_VAR_ExecuteCreateProject_bindParams[4].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindParams[4].buffer = (char *)DBOP_VAR_ExecuteCreateProject_noConstRealName;
    DBOP_VAR_ExecuteCreateProject_bindParams[4].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_noConstRealName);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreateProject_connect->stmt_add_project, DBOP_VAR_ExecuteCreateProject_bindParams)) {
        dzlog_error("[req: %s] Failed to bind insert param: %s", DBOP_VAR_ExecuteCreateProject_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateProject_connect->stmt_add_project));
        mysql_rollback(DBOP_VAR_ExecuteCreateProject_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteCreateProject_connect->mysql, 1);
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreateProject_connect->stmt_add_project)) {
        dzlog_error("[req: %s] Failed to execute insert statement: %s", DBOP_VAR_ExecuteCreateProject_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateProject_connect->stmt_add_project));
        mysql_rollback(DBOP_VAR_ExecuteCreateProject_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteCreateProject_connect->mysql, 1);
        return -1;
    }

    // 第二步：更新用户表的项目信息
    MYSQL_BIND DBOP_VAR_ExecuteCreateProject_bindUserParams[2];
    memset(DBOP_VAR_ExecuteCreateProject_bindUserParams, 0, sizeof(DBOP_VAR_ExecuteCreateProject_bindUserParams));

    // 绑定project_id
    DBOP_VAR_ExecuteCreateProject_bindUserParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindUserParams[0].buffer = (char *)DBOP_VAR_ExecuteCreateProject_noConstProjectId;
    DBOP_VAR_ExecuteCreateProject_bindUserParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_noConstProjectId);

    // 绑定user_id
    DBOP_VAR_ExecuteCreateProject_bindUserParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindUserParams[1].buffer = (char *)DBOP_VAR_ExecuteCreateProject_noConstUserId;
    DBOP_VAR_ExecuteCreateProject_bindUserParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_noConstUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreateProject_connect->stmt_update_user_project_info, DBOP_VAR_ExecuteCreateProject_bindUserParams)) {
        dzlog_error("[req: %s] Failed to bind user update param: %s", DBOP_VAR_ExecuteCreateProject_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateProject_connect->stmt_update_user_project_info));
        mysql_rollback(DBOP_VAR_ExecuteCreateProject_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteCreateProject_connect->mysql, 1);
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreateProject_connect->stmt_update_user_project_info)) {
        dzlog_error("[req: %s] Failed to execute user update statement: %s", DBOP_VAR_ExecuteCreateProject_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateProject_connect->stmt_update_user_project_info));
        mysql_rollback(DBOP_VAR_ExecuteCreateProject_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteCreateProject_connect->mysql, 1);
        return -1;
    }

    // 检查用户是否存在
    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteCreateProject_connect->stmt_update_user_project_info);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No user found with user_id: %s", DBOP_VAR_ExecuteCreateProject_requestId, DBOP_VAR_ExecuteCreateProject_userID);
        mysql_rollback(DBOP_VAR_ExecuteCreateProject_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteCreateProject_connect->mysql, 1);
        return 1; // 表示用户不存在
    }

    // 提交事务
    if (mysql_commit(DBOP_VAR_ExecuteCreateProject_connect->mysql)) {
        dzlog_error("[req: %s] Failed to commit transaction: %s", DBOP_VAR_ExecuteCreateProject_requestId, mysql_error(DBOP_VAR_ExecuteCreateProject_connect->mysql));
        mysql_rollback(DBOP_VAR_ExecuteCreateProject_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteCreateProject_connect->mysql, 1);
        return -1;
    }

    // 恢复自动提交
    mysql_autocommit(DBOP_VAR_ExecuteCreateProject_connect->mysql, 1);

    dzlog_info("[req: %s] Successfully created project and updated user info", DBOP_VAR_ExecuteCreateProject_requestId);
    return 0; // 表示成功
}

// 创建项目的API接口
void DBOP_FUN_ApiCreateProject(struct evhttp_request *DBOP_VAR_ApiCreateProject_request, void *DBOP_VAR_ApiCreateProject_voidCfg) {
    AppConfig *DBOP_VAR_ApiCreateProject_cfg = (AppConfig *)DBOP_VAR_ApiCreateProject_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiCreateProject_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiCreateProject_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiCreateProject.", DBOP_VAR_ApiCreateProject_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiCreateProject_request, DBOP_VAR_ApiCreateProject_cfg, DBOP_VAR_ApiCreateProject_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiCreateProject_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiCreateProject_request, DBOP_VAR_ApiCreateProject_requestId);
    if (!DBOP_VAR_ApiCreateProject_dataJsonAll) {
        return;
    }

    json_t *DBOP_VAR_ApiCreateProject_dataJsonProjectId = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiCreateProject_dataJsonUserId = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiCreateProject_dataJsonUserName = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "user_name");
    json_t *DBOP_VAR_ApiCreateProject_dataJsonUserAge = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "user_age");
    json_t *DBOP_VAR_ApiCreateProject_dataJsonRealName = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "real_name");

    if (!json_is_string(DBOP_VAR_ApiCreateProject_dataJsonProjectId) || 
        !json_is_string(DBOP_VAR_ApiCreateProject_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiCreateProject_dataJsonUserName) ||
        !json_is_integer(DBOP_VAR_ApiCreateProject_dataJsonUserAge) || 
        !json_is_string(DBOP_VAR_ApiCreateProject_dataJsonRealName)) {
        dzlog_error("[req: %s] Invalid JSON data received.", DBOP_VAR_ApiCreateProject_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiCreateProject_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiCreateProject_projectId = json_string_value(DBOP_VAR_ApiCreateProject_dataJsonProjectId);
    const char *DBOP_VAR_ApiCreateProject_userId = json_string_value(DBOP_VAR_ApiCreateProject_dataJsonUserId);
    const char *DBOP_VAR_ApiCreateProject_userName = json_string_value(DBOP_VAR_ApiCreateProject_dataJsonUserName);
    int DBOP_VAR_ApiCreateProject_userAge = json_integer_value(DBOP_VAR_ApiCreateProject_dataJsonUserAge);
    const char *DBOP_VAR_ApiCreateProject_realName = json_string_value(DBOP_VAR_ApiCreateProject_dataJsonRealName);

    dzlog_info("[req: %s] Executing database operation for ApiCreateProject: projectId=%s, userId=%s, userName=%s, userAge=%d, realName=%s", DBOP_VAR_ApiCreateProject_requestId, DBOP_VAR_ApiCreateProject_projectId, DBOP_VAR_ApiCreateProject_userId, DBOP_VAR_ApiCreateProject_userName, DBOP_VAR_ApiCreateProject_userAge, DBOP_VAR_ApiCreateProject_realName);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiCreateProject_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiCreateProject_cfg);
    int result = DBOP_FUN_ExecuteCreateProject(DBOP_VAR_ApiCreateProject_mysqlConnect, DBOP_VAR_ApiCreateProject_projectId, DBOP_VAR_ApiCreateProject_userId, DBOP_VAR_ApiCreateProject_userName, DBOP_VAR_ApiCreateProject_userAge, DBOP_VAR_ApiCreateProject_realName, DBOP_VAR_ApiCreateProject_requestId);

    // 发送响应
    if (result == 0) {
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, HTTP_OK, "OK", NULL);
        dzlog_info("[req: %s] Successfully created project, returning 200", DBOP_VAR_ApiCreateProject_requestId);
    } else if (result == 1) {
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, 404, "User not found", NULL);
        dzlog_warn("[req: %s] User not found, returning 404", DBOP_VAR_ApiCreateProject_requestId);
    } else {
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, HTTP_INTERNAL, "Internal Server Error", NULL);
        dzlog_error("[req: %s] Failed to create project, returning 500", DBOP_VAR_ApiCreateProject_requestId);
    }

    json_decref(DBOP_VAR_ApiCreateProject_dataJsonAll);
}

// ------------------------mysql创建project api逻辑结束----------------------------


// ------------------------mysql更新最后一次登录时间api逻辑开始----------------------------

// 更新最后一次登录时间的sql数据化输出
int DBOP_FUN_ExecuteUpdateLoginTime(DB_CONNECTION *DBOP_VAR_ExecuteUpdateLoginTime_connect, const char *DBOP_VAR_ExecuteUpdateLoginTime_userId, const char *DBOP_VAR_ExecuteUpdateLoginTime_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateLoginTime is starting", DBOP_VAR_ExecuteUpdateLoginTime_requestId);

    // 保护原始参数不被修改，复制一份用户ID作为更新的参数
    char DBOP_VAR_ExecuteUpdateLoginTime_noConstUserId[256];
    strncpy(DBOP_VAR_ExecuteUpdateLoginTime_noConstUserId, DBOP_VAR_ExecuteUpdateLoginTime_userId, sizeof(DBOP_VAR_ExecuteUpdateLoginTime_noConstUserId) - 1);
    DBOP_VAR_ExecuteUpdateLoginTime_noConstUserId[sizeof(DBOP_VAR_ExecuteUpdateLoginTime_noConstUserId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteUpdateLoginTime_bindParams[1];
    memset(DBOP_VAR_ExecuteUpdateLoginTime_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateLoginTime_bindParams));

    // 绑定user_id参数
    DBOP_VAR_ExecuteUpdateLoginTime_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateLoginTime_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteUpdateLoginTime_noConstUserId;
    DBOP_VAR_ExecuteUpdateLoginTime_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateLoginTime_noConstUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateLoginTime_connect->stmt_update_login_time, DBOP_VAR_ExecuteUpdateLoginTime_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateLoginTime_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateLoginTime_connect->stmt_update_login_time));
        return -1; // 表示绑定参数失败
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateLoginTime_connect->stmt_update_login_time)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateLoginTime_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateLoginTime_connect->stmt_update_login_time));
        return -1; // 表示执行语句失败
    }

    // 检查是否有行被更新
    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateLoginTime_connect->stmt_update_login_time);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No user found with user_id: %s", DBOP_VAR_ExecuteUpdateLoginTime_requestId, DBOP_VAR_ExecuteUpdateLoginTime_userId);
        return 1; // 表示用户不存在
    }

    dzlog_info("[req: %s] Successfully updated login time for user: %s", DBOP_VAR_ExecuteUpdateLoginTime_requestId, DBOP_VAR_ExecuteUpdateLoginTime_userId);
    return 0; // 表示成功
}

// 更新最后一次登录时间的API接口
void DBOP_FUN_ApiUpdateLoginTime(struct evhttp_request *DBOP_VAR_ApiUpdateLoginTime_request, void *DBOP_VAR_ApiUpdateLoginTime_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateLoginTime_cfg = (AppConfig *)DBOP_VAR_ApiUpdateLoginTime_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateLoginTime_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateLoginTime_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateLoginTime.", DBOP_VAR_ApiUpdateLoginTime_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateLoginTime_request, DBOP_VAR_ApiUpdateLoginTime_cfg, DBOP_VAR_ApiUpdateLoginTime_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateLoginTime_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateLoginTime_request, DBOP_VAR_ApiUpdateLoginTime_requestId);
    if (!DBOP_VAR_ApiUpdateLoginTime_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateLoginTime_dataJsonUserId = json_object_get(DBOP_VAR_ApiUpdateLoginTime_dataJsonAll, "user_id");
    if (!json_is_string(DBOP_VAR_ApiUpdateLoginTime_dataJsonUserId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'user_id'", DBOP_VAR_ApiUpdateLoginTime_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateLoginTime_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateLoginTime_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateLoginTime_userId = json_string_value(DBOP_VAR_ApiUpdateLoginTime_dataJsonUserId);
    dzlog_info("[req: %s] Executing database operation for ApiUpdateLoginTime: userId=%s", DBOP_VAR_ApiUpdateLoginTime_requestId, DBOP_VAR_ApiUpdateLoginTime_userId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateLoginTime_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateLoginTime_cfg);
    int result = DBOP_FUN_ExecuteUpdateLoginTime(DBOP_VAR_ApiUpdateLoginTime_mysqlConnect, DBOP_VAR_ApiUpdateLoginTime_userId, DBOP_VAR_ApiUpdateLoginTime_requestId);

    // 发送响应
    if (result == 0) {
        evhttp_send_reply(DBOP_VAR_ApiUpdateLoginTime_request, HTTP_OK, "OK", NULL);
        dzlog_info("[req: %s] Successfully updated login time, returning 200", DBOP_VAR_ApiUpdateLoginTime_requestId);
    } else if (result == 1) {
        evhttp_send_reply(DBOP_VAR_ApiUpdateLoginTime_request, 404, "User not found", NULL);
        dzlog_warn("[req: %s] User not found, returning 404", DBOP_VAR_ApiUpdateLoginTime_requestId);
    } else {
        evhttp_send_reply(DBOP_VAR_ApiUpdateLoginTime_request, HTTP_INTERNAL, "Internal Server Error", NULL);
        dzlog_error("[req: %s] Failed to update login time, returning 500", DBOP_VAR_ApiUpdateLoginTime_requestId);
    }

    json_decref(DBOP_VAR_ApiUpdateLoginTime_dataJsonAll);
}

// ------------------------mysql更新最后一次登录时间api逻辑结束----------------------------


// ------------------------mysql更新用户个性化信息api逻辑开始----------------------------

// 更新用户个性化信息的sql数据化输出
int DBOP_FUN_ExecuteUpdateUserInfo(DB_CONNECTION *DBOP_VAR_ExecuteUpdateUserInfo_connect, const char *DBOP_VAR_ExecuteUpdateUserInfo_userId, const char *DBOP_VAR_ExecuteUpdateUserInfo_userName, const char *DBOP_VAR_ExecuteUpdateUserInfo_avatar, const char *DBOP_VAR_ExecuteUpdateUserInfo_personalNote, const char *DBOP_VAR_ExecuteUpdateUserInfo_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateUserInfo is starting", DBOP_VAR_ExecuteUpdateUserInfo_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteUpdateUserInfo_noConstUserId[256];
    char DBOP_VAR_ExecuteUpdateUserInfo_noConstUserName[256];
    char DBOP_VAR_ExecuteUpdateUserInfo_noConstAvatar[256];
    char DBOP_VAR_ExecuteUpdateUserInfo_noConstPersonalNote[512];

    strncpy(DBOP_VAR_ExecuteUpdateUserInfo_noConstUserId, DBOP_VAR_ExecuteUpdateUserInfo_userId, sizeof(DBOP_VAR_ExecuteUpdateUserInfo_noConstUserId) - 1);
    DBOP_VAR_ExecuteUpdateUserInfo_noConstUserId[sizeof(DBOP_VAR_ExecuteUpdateUserInfo_noConstUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteUpdateUserInfo_noConstUserName, DBOP_VAR_ExecuteUpdateUserInfo_userName, sizeof(DBOP_VAR_ExecuteUpdateUserInfo_noConstUserName) - 1);
    DBOP_VAR_ExecuteUpdateUserInfo_noConstUserName[sizeof(DBOP_VAR_ExecuteUpdateUserInfo_noConstUserName) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteUpdateUserInfo_noConstAvatar, DBOP_VAR_ExecuteUpdateUserInfo_avatar, sizeof(DBOP_VAR_ExecuteUpdateUserInfo_noConstAvatar) - 1);
    DBOP_VAR_ExecuteUpdateUserInfo_noConstAvatar[sizeof(DBOP_VAR_ExecuteUpdateUserInfo_noConstAvatar) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteUpdateUserInfo_noConstPersonalNote, DBOP_VAR_ExecuteUpdateUserInfo_personalNote, sizeof(DBOP_VAR_ExecuteUpdateUserInfo_noConstPersonalNote) - 1);
    DBOP_VAR_ExecuteUpdateUserInfo_noConstPersonalNote[sizeof(DBOP_VAR_ExecuteUpdateUserInfo_noConstPersonalNote) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteUpdateUserInfo_bindParams[4];
    memset(DBOP_VAR_ExecuteUpdateUserInfo_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateUserInfo_bindParams));

    // 绑定参数：user_name, avatar, personal_note, user_id
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteUpdateUserInfo_noConstUserName;
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserInfo_noConstUserName);

    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteUpdateUserInfo_noConstAvatar;
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserInfo_noConstAvatar);

    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[2].buffer = (char *)DBOP_VAR_ExecuteUpdateUserInfo_noConstPersonalNote;
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[2].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserInfo_noConstPersonalNote);

    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[3].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[3].buffer = (char *)DBOP_VAR_ExecuteUpdateUserInfo_noConstUserId;
    DBOP_VAR_ExecuteUpdateUserInfo_bindParams[3].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserInfo_noConstUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateUserInfo_connect->stmt_update_user_info, DBOP_VAR_ExecuteUpdateUserInfo_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateUserInfo_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserInfo_connect->stmt_update_user_info));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateUserInfo_connect->stmt_update_user_info)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateUserInfo_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserInfo_connect->stmt_update_user_info));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateUserInfo_connect->stmt_update_user_info);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No user found with user_id: %s", DBOP_VAR_ExecuteUpdateUserInfo_requestId, DBOP_VAR_ExecuteUpdateUserInfo_userId);
        return 1;
    }

    dzlog_info("[req: %s] Successfully updated user info for user: %s", DBOP_VAR_ExecuteUpdateUserInfo_requestId, DBOP_VAR_ExecuteUpdateUserInfo_userId);
    return 0;
}

// 更新用户个性化信息的API接口
void DBOP_FUN_ApiUpdateUserInfo(struct evhttp_request *DBOP_VAR_ApiUpdateUserInfo_request, void *DBOP_VAR_ApiUpdateUserInfo_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateUserInfo_cfg = (AppConfig *)DBOP_VAR_ApiUpdateUserInfo_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateUserInfo_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateUserInfo_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateUserInfo.", DBOP_VAR_ApiUpdateUserInfo_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateUserInfo_request, DBOP_VAR_ApiUpdateUserInfo_cfg, DBOP_VAR_ApiUpdateUserInfo_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateUserInfo_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateUserInfo_request, DBOP_VAR_ApiUpdateUserInfo_requestId);
    if (!DBOP_VAR_ApiUpdateUserInfo_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateUserInfo_dataJsonUserId = json_object_get(DBOP_VAR_ApiUpdateUserInfo_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiUpdateUserInfo_dataJsonUserName = json_object_get(DBOP_VAR_ApiUpdateUserInfo_dataJsonAll, "user_name");
    json_t *DBOP_VAR_ApiUpdateUserInfo_dataJsonAvatar = json_object_get(DBOP_VAR_ApiUpdateUserInfo_dataJsonAll, "avatar");
    json_t *DBOP_VAR_ApiUpdateUserInfo_dataJsonPersonalNote = json_object_get(DBOP_VAR_ApiUpdateUserInfo_dataJsonAll, "personal_note");

    if (!json_is_string(DBOP_VAR_ApiUpdateUserInfo_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiUpdateUserInfo_dataJsonUserName) ||
        !json_is_string(DBOP_VAR_ApiUpdateUserInfo_dataJsonAvatar) ||
        !json_is_string(DBOP_VAR_ApiUpdateUserInfo_dataJsonPersonalNote)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string types for all fields", DBOP_VAR_ApiUpdateUserInfo_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateUserInfo_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateUserInfo_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateUserInfo_userId = json_string_value(DBOP_VAR_ApiUpdateUserInfo_dataJsonUserId);
    const char *DBOP_VAR_ApiUpdateUserInfo_userName = json_string_value(DBOP_VAR_ApiUpdateUserInfo_dataJsonUserName);
    const char *DBOP_VAR_ApiUpdateUserInfo_avatar = json_string_value(DBOP_VAR_ApiUpdateUserInfo_dataJsonAvatar);
    const char *DBOP_VAR_ApiUpdateUserInfo_personalNote = json_string_value(DBOP_VAR_ApiUpdateUserInfo_dataJsonPersonalNote);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateUserInfo: userId=%s, userName=%s", DBOP_VAR_ApiUpdateUserInfo_requestId, DBOP_VAR_ApiUpdateUserInfo_userId, DBOP_VAR_ApiUpdateUserInfo_userName);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateUserInfo_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateUserInfo_cfg);
    int result = DBOP_FUN_ExecuteUpdateUserInfo(DBOP_VAR_ApiUpdateUserInfo_mysqlConnect, DBOP_VAR_ApiUpdateUserInfo_userId, DBOP_VAR_ApiUpdateUserInfo_userName, DBOP_VAR_ApiUpdateUserInfo_avatar, DBOP_VAR_ApiUpdateUserInfo_personalNote, DBOP_VAR_ApiUpdateUserInfo_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateUserInfo_request, result, DBOP_VAR_ApiUpdateUserInfo_requestId, "update user info");

    json_decref(DBOP_VAR_ApiUpdateUserInfo_dataJsonAll);
}

// ------------------------mysql更新用户个性化信息api逻辑结束----------------------------


// ------------------------mysql更新用户手机号api逻辑开始----------------------------

// 更新用户手机号的sql数据化输出
int DBOP_FUN_ExecuteUpdateUserPhone(DB_CONNECTION *DBOP_VAR_ExecuteUpdateUserPhone_connect, const char *DBOP_VAR_ExecuteUpdateUserPhone_userId, const char *DBOP_VAR_ExecuteUpdateUserPhone_phoneNumber, const char *DBOP_VAR_ExecuteUpdateUserPhone_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateUserPhone is starting", DBOP_VAR_ExecuteUpdateUserPhone_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteUpdateUserPhone_noConstUserId[256];
    char DBOP_VAR_ExecuteUpdateUserPhone_noConstPhoneNumber[256];

    strncpy(DBOP_VAR_ExecuteUpdateUserPhone_noConstUserId, DBOP_VAR_ExecuteUpdateUserPhone_userId, sizeof(DBOP_VAR_ExecuteUpdateUserPhone_noConstUserId) - 1);
    DBOP_VAR_ExecuteUpdateUserPhone_noConstUserId[sizeof(DBOP_VAR_ExecuteUpdateUserPhone_noConstUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteUpdateUserPhone_noConstPhoneNumber, DBOP_VAR_ExecuteUpdateUserPhone_phoneNumber, sizeof(DBOP_VAR_ExecuteUpdateUserPhone_noConstPhoneNumber) - 1);
    DBOP_VAR_ExecuteUpdateUserPhone_noConstPhoneNumber[sizeof(DBOP_VAR_ExecuteUpdateUserPhone_noConstPhoneNumber) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteUpdateUserPhone_bindParams[2];
    memset(DBOP_VAR_ExecuteUpdateUserPhone_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateUserPhone_bindParams));

    // 绑定参数：phone_number, user_id
    DBOP_VAR_ExecuteUpdateUserPhone_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserPhone_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteUpdateUserPhone_noConstPhoneNumber;
    DBOP_VAR_ExecuteUpdateUserPhone_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserPhone_noConstPhoneNumber);

    DBOP_VAR_ExecuteUpdateUserPhone_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserPhone_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteUpdateUserPhone_noConstUserId;
    DBOP_VAR_ExecuteUpdateUserPhone_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserPhone_noConstUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateUserPhone_connect->stmt_update_user_phone, DBOP_VAR_ExecuteUpdateUserPhone_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateUserPhone_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserPhone_connect->stmt_update_user_phone));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateUserPhone_connect->stmt_update_user_phone)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateUserPhone_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserPhone_connect->stmt_update_user_phone));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateUserPhone_connect->stmt_update_user_phone);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No user found with user_id: %s", DBOP_VAR_ExecuteUpdateUserPhone_requestId, DBOP_VAR_ExecuteUpdateUserPhone_userId);
        return 1;
    }

    dzlog_info("[req: %s] Successfully updated phone number for user: %s", DBOP_VAR_ExecuteUpdateUserPhone_requestId, DBOP_VAR_ExecuteUpdateUserPhone_userId);
    return 0;
}

// 更新用户手机号的API接口
void DBOP_FUN_ApiUpdateUserPhone(struct evhttp_request *DBOP_VAR_ApiUpdateUserPhone_request, void *DBOP_VAR_ApiUpdateUserPhone_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateUserPhone_cfg = (AppConfig *)DBOP_VAR_ApiUpdateUserPhone_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateUserPhone_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateUserPhone_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateUserPhone.", DBOP_VAR_ApiUpdateUserPhone_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateUserPhone_request, DBOP_VAR_ApiUpdateUserPhone_cfg, DBOP_VAR_ApiUpdateUserPhone_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateUserPhone_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateUserPhone_request, DBOP_VAR_ApiUpdateUserPhone_requestId);
    if (!DBOP_VAR_ApiUpdateUserPhone_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateUserPhone_dataJsonUserId = json_object_get(DBOP_VAR_ApiUpdateUserPhone_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiUpdateUserPhone_dataJsonPhoneNumber = json_object_get(DBOP_VAR_ApiUpdateUserPhone_dataJsonAll, "phone_number");

    if (!json_is_string(DBOP_VAR_ApiUpdateUserPhone_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiUpdateUserPhone_dataJsonPhoneNumber)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string types for 'user_id' and 'phone_number'", DBOP_VAR_ApiUpdateUserPhone_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateUserPhone_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateUserPhone_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateUserPhone_userId = json_string_value(DBOP_VAR_ApiUpdateUserPhone_dataJsonUserId);
    const char *DBOP_VAR_ApiUpdateUserPhone_phoneNumber = json_string_value(DBOP_VAR_ApiUpdateUserPhone_dataJsonPhoneNumber);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateUserPhone: userId=%s, phoneNumber=%s", DBOP_VAR_ApiUpdateUserPhone_requestId, DBOP_VAR_ApiUpdateUserPhone_userId, DBOP_VAR_ApiUpdateUserPhone_phoneNumber);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateUserPhone_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateUserPhone_cfg);
    int result = DBOP_FUN_ExecuteUpdateUserPhone(DBOP_VAR_ApiUpdateUserPhone_mysqlConnect, DBOP_VAR_ApiUpdateUserPhone_userId, DBOP_VAR_ApiUpdateUserPhone_phoneNumber, DBOP_VAR_ApiUpdateUserPhone_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateUserPhone_request, result, DBOP_VAR_ApiUpdateUserPhone_requestId, "update user phone");

    json_decref(DBOP_VAR_ApiUpdateUserPhone_dataJsonAll);
}

// ------------------------mysql更新用户手机号api逻辑结束----------------------------


// ------------------------mysql更新用户微信ID api逻辑开始----------------------------

// 更新用户微信ID的sql数据化输出
int DBOP_FUN_ExecuteUpdateUserWeixinId(DB_CONNECTION *DBOP_VAR_ExecuteUpdateUserWeixinId_connect, const char *DBOP_VAR_ExecuteUpdateUserWeixinId_userId, const char *DBOP_VAR_ExecuteUpdateUserWeixinId_weixinOpenid, const char *DBOP_VAR_ExecuteUpdateUserWeixinId_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateUserWeixinId is starting", DBOP_VAR_ExecuteUpdateUserWeixinId_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteUpdateUserWeixinId_noConstUserId[256];
    char DBOP_VAR_ExecuteUpdateUserWeixinId_noConstWeixinOpenid[256];

    strncpy(DBOP_VAR_ExecuteUpdateUserWeixinId_noConstUserId, DBOP_VAR_ExecuteUpdateUserWeixinId_userId, sizeof(DBOP_VAR_ExecuteUpdateUserWeixinId_noConstUserId) - 1);
    DBOP_VAR_ExecuteUpdateUserWeixinId_noConstUserId[sizeof(DBOP_VAR_ExecuteUpdateUserWeixinId_noConstUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteUpdateUserWeixinId_noConstWeixinOpenid, DBOP_VAR_ExecuteUpdateUserWeixinId_weixinOpenid, sizeof(DBOP_VAR_ExecuteUpdateUserWeixinId_noConstWeixinOpenid) - 1);
    DBOP_VAR_ExecuteUpdateUserWeixinId_noConstWeixinOpenid[sizeof(DBOP_VAR_ExecuteUpdateUserWeixinId_noConstWeixinOpenid) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams[2];
    memset(DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams));

    // 绑定参数：weixin_openid, user_id
    DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteUpdateUserWeixinId_noConstWeixinOpenid;
    DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserWeixinId_noConstWeixinOpenid);

    DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteUpdateUserWeixinId_noConstUserId;
    DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserWeixinId_noConstUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateUserWeixinId_connect->stmt_update_user_weixinid, DBOP_VAR_ExecuteUpdateUserWeixinId_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateUserWeixinId_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserWeixinId_connect->stmt_update_user_weixinid));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateUserWeixinId_connect->stmt_update_user_weixinid)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateUserWeixinId_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserWeixinId_connect->stmt_update_user_weixinid));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateUserWeixinId_connect->stmt_update_user_weixinid);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No user found with user_id: %s", DBOP_VAR_ExecuteUpdateUserWeixinId_requestId, DBOP_VAR_ExecuteUpdateUserWeixinId_userId);
        return 1;
    }

    dzlog_info("[req: %s] Successfully updated weixin openid for user: %s", DBOP_VAR_ExecuteUpdateUserWeixinId_requestId, DBOP_VAR_ExecuteUpdateUserWeixinId_userId);
    return 0;
}

// 更新用户微信ID的API接口
void DBOP_FUN_ApiUpdateUserWeixinId(struct evhttp_request *DBOP_VAR_ApiUpdateUserWeixinId_request, void *DBOP_VAR_ApiUpdateUserWeixinId_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateUserWeixinId_cfg = (AppConfig *)DBOP_VAR_ApiUpdateUserWeixinId_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateUserWeixinId_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateUserWeixinId_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateUserWeixinId.", DBOP_VAR_ApiUpdateUserWeixinId_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateUserWeixinId_request, DBOP_VAR_ApiUpdateUserWeixinId_cfg, DBOP_VAR_ApiUpdateUserWeixinId_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateUserWeixinId_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateUserWeixinId_request, DBOP_VAR_ApiUpdateUserWeixinId_requestId);
    if (!DBOP_VAR_ApiUpdateUserWeixinId_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateUserWeixinId_dataJsonUserId = json_object_get(DBOP_VAR_ApiUpdateUserWeixinId_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiUpdateUserWeixinId_dataJsonWeixinOpenid = json_object_get(DBOP_VAR_ApiUpdateUserWeixinId_dataJsonAll, "weixin_openid");

    if (!json_is_string(DBOP_VAR_ApiUpdateUserWeixinId_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiUpdateUserWeixinId_dataJsonWeixinOpenid)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string types for 'user_id' and 'weixin_openid'", DBOP_VAR_ApiUpdateUserWeixinId_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateUserWeixinId_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateUserWeixinId_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateUserWeixinId_userId = json_string_value(DBOP_VAR_ApiUpdateUserWeixinId_dataJsonUserId);
    const char *DBOP_VAR_ApiUpdateUserWeixinId_weixinOpenid = json_string_value(DBOP_VAR_ApiUpdateUserWeixinId_dataJsonWeixinOpenid);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateUserWeixinId: userId=%s, weixinOpenid=%s", DBOP_VAR_ApiUpdateUserWeixinId_requestId, DBOP_VAR_ApiUpdateUserWeixinId_userId, DBOP_VAR_ApiUpdateUserWeixinId_weixinOpenid);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateUserWeixinId_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateUserWeixinId_cfg);
    int result = DBOP_FUN_ExecuteUpdateUserWeixinId(DBOP_VAR_ApiUpdateUserWeixinId_mysqlConnect, DBOP_VAR_ApiUpdateUserWeixinId_userId, DBOP_VAR_ApiUpdateUserWeixinId_weixinOpenid, DBOP_VAR_ApiUpdateUserWeixinId_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateUserWeixinId_request, result, DBOP_VAR_ApiUpdateUserWeixinId_requestId, "update user weixin id");

    json_decref(DBOP_VAR_ApiUpdateUserWeixinId_dataJsonAll);
}

// ------------------------mysql更新用户微信ID api逻辑结束----------------------------


// ------------------------mysql更新用户权限组api逻辑开始----------------------------

// 更新用户权限组的sql数据化输出
int DBOP_FUN_ExecuteUpdateUserPermission(DB_CONNECTION *DBOP_VAR_ExecuteUpdateUserPermission_connect, const char *DBOP_VAR_ExecuteUpdateUserPermission_userId, int DBOP_VAR_ExecuteUpdateUserPermission_userPermission, const char *DBOP_VAR_ExecuteUpdateUserPermission_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateUserPermission is starting", DBOP_VAR_ExecuteUpdateUserPermission_requestId);

    // 保护原始参数不被修改，复制用户ID
    char DBOP_VAR_ExecuteUpdateUserPermission_noConstUserId[256];
    strncpy(DBOP_VAR_ExecuteUpdateUserPermission_noConstUserId, DBOP_VAR_ExecuteUpdateUserPermission_userId, sizeof(DBOP_VAR_ExecuteUpdateUserPermission_noConstUserId) - 1);
    DBOP_VAR_ExecuteUpdateUserPermission_noConstUserId[sizeof(DBOP_VAR_ExecuteUpdateUserPermission_noConstUserId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteUpdateUserPermission_bindParams[2];
    memset(DBOP_VAR_ExecuteUpdateUserPermission_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateUserPermission_bindParams));

    // 绑定参数：user_permission, user_id
    DBOP_VAR_ExecuteUpdateUserPermission_bindParams[0].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteUpdateUserPermission_bindParams[0].buffer = &DBOP_VAR_ExecuteUpdateUserPermission_userPermission;
    DBOP_VAR_ExecuteUpdateUserPermission_bindParams[0].buffer_length = sizeof(DBOP_VAR_ExecuteUpdateUserPermission_userPermission);

    DBOP_VAR_ExecuteUpdateUserPermission_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserPermission_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteUpdateUserPermission_noConstUserId;
    DBOP_VAR_ExecuteUpdateUserPermission_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserPermission_noConstUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateUserPermission_connect->stmt_update_user_permission, DBOP_VAR_ExecuteUpdateUserPermission_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateUserPermission_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserPermission_connect->stmt_update_user_permission));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateUserPermission_connect->stmt_update_user_permission)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateUserPermission_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserPermission_connect->stmt_update_user_permission));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateUserPermission_connect->stmt_update_user_permission);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No user found with user_id: %s", DBOP_VAR_ExecuteUpdateUserPermission_requestId, DBOP_VAR_ExecuteUpdateUserPermission_userId);
        return 1;
    }

    dzlog_info("[req: %s] Successfully updated permission for user: %s to permission: %d", DBOP_VAR_ExecuteUpdateUserPermission_requestId, DBOP_VAR_ExecuteUpdateUserPermission_userId, DBOP_VAR_ExecuteUpdateUserPermission_userPermission);
    return 0;
}

// 更新用户权限组的API接口
void DBOP_FUN_ApiUpdateUserPermission(struct evhttp_request *DBOP_VAR_ApiUpdateUserPermission_request, void *DBOP_VAR_ApiUpdateUserPermission_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateUserPermission_cfg = (AppConfig *)DBOP_VAR_ApiUpdateUserPermission_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateUserPermission_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateUserPermission_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateUserPermission.", DBOP_VAR_ApiUpdateUserPermission_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateUserPermission_request, DBOP_VAR_ApiUpdateUserPermission_cfg, DBOP_VAR_ApiUpdateUserPermission_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateUserPermission_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateUserPermission_request, DBOP_VAR_ApiUpdateUserPermission_requestId);
    if (!DBOP_VAR_ApiUpdateUserPermission_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateUserPermission_dataJsonUserId = json_object_get(DBOP_VAR_ApiUpdateUserPermission_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiUpdateUserPermission_dataJsonUserPermission = json_object_get(DBOP_VAR_ApiUpdateUserPermission_dataJsonAll, "user_permission");

    if (!json_is_string(DBOP_VAR_ApiUpdateUserPermission_dataJsonUserId) ||
        !json_is_integer(DBOP_VAR_ApiUpdateUserPermission_dataJsonUserPermission)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'user_id' and integer type for 'user_permission'", DBOP_VAR_ApiUpdateUserPermission_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateUserPermission_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateUserPermission_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateUserPermission_userId = json_string_value(DBOP_VAR_ApiUpdateUserPermission_dataJsonUserId);
    int DBOP_VAR_ApiUpdateUserPermission_userPermission = json_integer_value(DBOP_VAR_ApiUpdateUserPermission_dataJsonUserPermission);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateUserPermission: userId=%s, userPermission=%d", DBOP_VAR_ApiUpdateUserPermission_requestId, DBOP_VAR_ApiUpdateUserPermission_userId, DBOP_VAR_ApiUpdateUserPermission_userPermission);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateUserPermission_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateUserPermission_cfg);
    int result = DBOP_FUN_ExecuteUpdateUserPermission(DBOP_VAR_ApiUpdateUserPermission_mysqlConnect, DBOP_VAR_ApiUpdateUserPermission_userId, DBOP_VAR_ApiUpdateUserPermission_userPermission, DBOP_VAR_ApiUpdateUserPermission_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateUserPermission_request, result, DBOP_VAR_ApiUpdateUserPermission_requestId, "update user permission");

    json_decref(DBOP_VAR_ApiUpdateUserPermission_dataJsonAll);
}

// ------------------------mysql更新用户权限组api逻辑结束----------------------------


// ------------------------mysql修改用户密码api逻辑开始----------------------------

// 修改用户密码的sql数据化输出
int DBOP_FUN_ExecuteUpdateUserPasswd(DB_CONNECTION *DBOP_VAR_ExecuteUpdateUserPasswd_connect, const char *DBOP_VAR_ExecuteUpdateUserPasswd_userId, const char *DBOP_VAR_ExecuteUpdateUserPasswd_passwd, const char *DBOP_VAR_ExecuteUpdateUserPasswd_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateUserPasswd is starting", DBOP_VAR_ExecuteUpdateUserPasswd_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteUpdateUserPasswd_noConstUserId[256];
    char DBOP_VAR_ExecuteUpdateUserPasswd_noConstPasswd[256];

    strncpy(DBOP_VAR_ExecuteUpdateUserPasswd_noConstUserId, DBOP_VAR_ExecuteUpdateUserPasswd_userId, sizeof(DBOP_VAR_ExecuteUpdateUserPasswd_noConstUserId) - 1);
    DBOP_VAR_ExecuteUpdateUserPasswd_noConstUserId[sizeof(DBOP_VAR_ExecuteUpdateUserPasswd_noConstUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteUpdateUserPasswd_noConstPasswd, DBOP_VAR_ExecuteUpdateUserPasswd_passwd, sizeof(DBOP_VAR_ExecuteUpdateUserPasswd_noConstPasswd) - 1);
    DBOP_VAR_ExecuteUpdateUserPasswd_noConstPasswd[sizeof(DBOP_VAR_ExecuteUpdateUserPasswd_noConstPasswd) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteUpdateUserPasswd_bindParams[2];
    memset(DBOP_VAR_ExecuteUpdateUserPasswd_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateUserPasswd_bindParams));

    // 绑定参数：user_password, user_id
    DBOP_VAR_ExecuteUpdateUserPasswd_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserPasswd_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteUpdateUserPasswd_noConstPasswd;
    DBOP_VAR_ExecuteUpdateUserPasswd_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserPasswd_noConstPasswd);

    DBOP_VAR_ExecuteUpdateUserPasswd_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateUserPasswd_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteUpdateUserPasswd_noConstUserId;
    DBOP_VAR_ExecuteUpdateUserPasswd_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateUserPasswd_noConstUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateUserPasswd_connect->stmt_update_user_passwd, DBOP_VAR_ExecuteUpdateUserPasswd_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateUserPasswd_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserPasswd_connect->stmt_update_user_passwd));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateUserPasswd_connect->stmt_update_user_passwd)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateUserPasswd_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateUserPasswd_connect->stmt_update_user_passwd));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateUserPasswd_connect->stmt_update_user_passwd);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No user found with user_id: %s", DBOP_VAR_ExecuteUpdateUserPasswd_requestId, DBOP_VAR_ExecuteUpdateUserPasswd_userId);
        return 1;
    }

    dzlog_info("[req: %s] Successfully updated password for user: %s", DBOP_VAR_ExecuteUpdateUserPasswd_requestId, DBOP_VAR_ExecuteUpdateUserPasswd_userId);
    return 0;
}

// 修改用户密码的API接口
void DBOP_FUN_ApiUpdateUserPasswd(struct evhttp_request *DBOP_VAR_ApiUpdateUserPasswd_request, void *DBOP_VAR_ApiUpdateUserPasswd_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateUserPasswd_cfg = (AppConfig *)DBOP_VAR_ApiUpdateUserPasswd_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateUserPasswd_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateUserPasswd_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateUserPasswd.", DBOP_VAR_ApiUpdateUserPasswd_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateUserPasswd_request, DBOP_VAR_ApiUpdateUserPasswd_cfg, DBOP_VAR_ApiUpdateUserPasswd_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateUserPasswd_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateUserPasswd_request, DBOP_VAR_ApiUpdateUserPasswd_requestId);
    if (!DBOP_VAR_ApiUpdateUserPasswd_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateUserPasswd_dataJsonUserId = json_object_get(DBOP_VAR_ApiUpdateUserPasswd_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiUpdateUserPasswd_dataJsonPasswd = json_object_get(DBOP_VAR_ApiUpdateUserPasswd_dataJsonAll, "passwd");

    if (!json_is_string(DBOP_VAR_ApiUpdateUserPasswd_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiUpdateUserPasswd_dataJsonPasswd)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string types for 'user_id' and 'passwd'", DBOP_VAR_ApiUpdateUserPasswd_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateUserPasswd_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateUserPasswd_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateUserPasswd_userId = json_string_value(DBOP_VAR_ApiUpdateUserPasswd_dataJsonUserId);
    const char *DBOP_VAR_ApiUpdateUserPasswd_passwd = json_string_value(DBOP_VAR_ApiUpdateUserPasswd_dataJsonPasswd);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateUserPasswd: userId=%s", DBOP_VAR_ApiUpdateUserPasswd_requestId, DBOP_VAR_ApiUpdateUserPasswd_userId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateUserPasswd_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateUserPasswd_cfg);
    int result = DBOP_FUN_ExecuteUpdateUserPasswd(DBOP_VAR_ApiUpdateUserPasswd_mysqlConnect, DBOP_VAR_ApiUpdateUserPasswd_userId, DBOP_VAR_ApiUpdateUserPasswd_passwd, DBOP_VAR_ApiUpdateUserPasswd_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateUserPasswd_request, result, DBOP_VAR_ApiUpdateUserPasswd_requestId, "update user password");

    json_decref(DBOP_VAR_ApiUpdateUserPasswd_dataJsonAll);
}

// ------------------------mysql修改用户密码api逻辑结束----------------------------


// ------------------------mysql根据servicename查询对应servicepassword逻辑开始----------------------------


//根据servicename查询对应密码
char* DBOP_FUN_ExecuteGetServicePassword(DB_CONNECTION *DBOP_VAR_ExecuteGetServicePassword_mysqlConnect, const char *DBOP_VAR_ExecuteGetServicePassword_waitGetServiceName) {
    dzlog_info("DBOP_FUN_ExecuteGetServicePassword is doing, service name is: %s", DBOP_VAR_ExecuteGetServicePassword_waitGetServiceName);
    // 保护原始参数不被修改，复制一份服务名称作为查询的参数
    char DBOP_VAR_ExecuteGetServicePassword_notConstServiceName[256];
    strncpy(DBOP_VAR_ExecuteGetServicePassword_notConstServiceName, DBOP_VAR_ExecuteGetServicePassword_waitGetServiceName, sizeof(DBOP_VAR_ExecuteGetServicePassword_notConstServiceName) - 1);
    DBOP_VAR_ExecuteGetServicePassword_notConstServiceName[sizeof(DBOP_VAR_ExecuteGetServicePassword_notConstServiceName) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteGetServicePassword_bindParams[1];
    memset(DBOP_VAR_ExecuteGetServicePassword_bindParams, 0, sizeof(DBOP_VAR_ExecuteGetServicePassword_bindParams));

    DBOP_VAR_ExecuteGetServicePassword_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteGetServicePassword_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteGetServicePassword_notConstServiceName;
    DBOP_VAR_ExecuteGetServicePassword_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteGetServicePassword_notConstServiceName); 

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd, DBOP_VAR_ExecuteGetServicePassword_bindParams)) {
        dzlog_error("Failed to bind param: %s", mysql_stmt_error(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd));
        return NULL;
    }

    MYSQL_BIND DBOP_VAR_ExecuteGetServicePassword_bindResult[1];
    memset(DBOP_VAR_ExecuteGetServicePassword_bindResult, 0, sizeof(DBOP_VAR_ExecuteGetServicePassword_bindResult));
    char DBOP_VAR_ExecuteGetServicePassword_servicePasswd[256];
    unsigned long DBOP_VAR_ExecuteGetServicePassword_length;

    DBOP_VAR_ExecuteGetServicePassword_bindResult[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteGetServicePassword_bindResult[0].buffer = DBOP_VAR_ExecuteGetServicePassword_servicePasswd;
    DBOP_VAR_ExecuteGetServicePassword_bindResult[0].buffer_length = sizeof(DBOP_VAR_ExecuteGetServicePassword_servicePasswd);
    DBOP_VAR_ExecuteGetServicePassword_bindResult[0].length = &DBOP_VAR_ExecuteGetServicePassword_length;

    if (mysql_stmt_execute(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd)) {
        dzlog_error("Failed to execute DBOP_FUN_ExecuteGetServicePassword: %s", mysql_stmt_error(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd));
        return NULL;
    }

    mysql_stmt_store_result(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd);

    if (mysql_stmt_bind_result(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd, DBOP_VAR_ExecuteGetServicePassword_bindResult)) {
        dzlog_error("Failed to bind result: %s", mysql_stmt_error(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd));
        return NULL;
    }

    // 获取查询结果
    if (mysql_stmt_fetch(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd) == MYSQL_NO_DATA) {
        dzlog_info("No password found for service: %s", DBOP_VAR_ExecuteGetServicePassword_waitGetServiceName);
        return NULL;
    }

    // 确保字符串正确终止
    DBOP_VAR_ExecuteGetServicePassword_servicePasswd[DBOP_VAR_ExecuteGetServicePassword_length] = '\0';

    dzlog_info("Successfully retrieved service password for: %s", DBOP_VAR_ExecuteGetServicePassword_waitGetServiceName);

    // 为返回的密码分配内存
    char* DBOP_VAR_ExecuteGetServicePassword_passwordResult = malloc(DBOP_VAR_ExecuteGetServicePassword_length + 1);
    if (DBOP_VAR_ExecuteGetServicePassword_passwordResult != NULL) {
        strncpy(DBOP_VAR_ExecuteGetServicePassword_passwordResult, DBOP_VAR_ExecuteGetServicePassword_servicePasswd, DBOP_VAR_ExecuteGetServicePassword_length + 1);
    } else {
        dzlog_error("Memory allocation failed for password result");
        return NULL;
    }
    mysql_stmt_free_result(DBOP_VAR_ExecuteGetServicePassword_mysqlConnect->stmt_get_service_passwd);

    return DBOP_VAR_ExecuteGetServicePassword_passwordResult;
}


// 查询service对应密码的虚拟路径执行函数
void DBOP_FUN_ApiGetServicePasswd(struct evhttp_request *DBOP_VAR_ApiGetServicePasswd_request, void *DBOP_VAR_ApiGetServicePasswd_voidCfg) {
    AppConfig *DBOP_VAR_ApiGetServicePasswd_cfg = (AppConfig *)DBOP_VAR_ApiGetServicePasswd_voidCfg;
    const char *DBOP_VAR_ApiGetServicePasswd_requestId = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiGetServicePasswd_request), "X-Request-ID");
    char uuid_str[37];  // UUID字符串的长度
    if (!DBOP_VAR_ApiGetServicePasswd_requestId) {
        // 如果请求中没有X-Request-ID头部，生成一个UUID作为请求ID
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, uuid_str);
        DBOP_VAR_ApiGetServicePasswd_requestId = uuid_str;
    }
    dzlog_info("Processing API request to ApiGetServicePasswd, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);
    // 解析 HTTP 请求中的查询字符串并存储在 DBOP_VAR_ApiGetServicePasswd_headers 结构体中
    struct evkeyvalq DBOP_VAR_ApiGetServicePasswd_headers;
    evhttp_parse_query_str(evhttp_uri_get_query(evhttp_request_get_evhttp_uri(DBOP_VAR_ApiGetServicePasswd_request)), &DBOP_VAR_ApiGetServicePasswd_headers);
    dzlog_info("Parsing query string from request, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);
    // 请求鉴权
    const char *DBOP_VAR_ApiGetServicePasswd_serviceName = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiGetServicePasswd_request), "ServiceName");
    const char *DBOP_VAR_ApiGetServicePasswd_token = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiGetServicePasswd_request), "Authorization");
    if (DBOP_FUN_AuthenticateRequest(DBOP_VAR_ApiGetServicePasswd_cfg, DBOP_VAR_ApiGetServicePasswd_serviceName, DBOP_VAR_ApiGetServicePasswd_token)) {
        dzlog_info("Request authentication approval, request id: %s .", DBOP_VAR_ApiGetServicePasswd_requestId);
    } else {
        dzlog_warn("Unauthorized access attempt, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);
        evhttp_send_reply(DBOP_VAR_ApiGetServicePasswd_request, 401, "Unauthorized", NULL);
        return;
    }
    // 从 POST 数据中读取 JSON 参数
    struct evbuffer *DBOP_VAR_ApiGetServicePasswd_inputBuffer = evhttp_request_get_input_buffer(DBOP_VAR_ApiGetServicePasswd_request);
    size_t DBOP_VAR_ApiGetServicePasswd_bufferLen = evbuffer_get_length(DBOP_VAR_ApiGetServicePasswd_inputBuffer);
    char DBOP_VAR_ApiGetServicePasswd_postData[DBOP_VAR_ApiGetServicePasswd_bufferLen + 1];
    evbuffer_remove(DBOP_VAR_ApiGetServicePasswd_inputBuffer, DBOP_VAR_ApiGetServicePasswd_postData, DBOP_VAR_ApiGetServicePasswd_bufferLen);
    DBOP_VAR_ApiGetServicePasswd_postData[DBOP_VAR_ApiGetServicePasswd_bufferLen] = '\0';

    json_error_t DBOP_VAR_ApiGetServicePasswd_dataJsonError;
    json_t *DBOP_VAR_ApiGetServicePasswd_dataJsonAll = json_loads(DBOP_VAR_ApiGetServicePasswd_postData, 0, &DBOP_VAR_ApiGetServicePasswd_dataJsonError);
    dzlog_info("Parsing POST data, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);
    if (!DBOP_VAR_ApiGetServicePasswd_dataJsonAll) {
        dzlog_error("Failed to parse JSON from request body, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);
        evhttp_send_reply(DBOP_VAR_ApiGetServicePasswd_request, 400, "Bad Request", NULL);
        return;
    }

    json_t *DBOP_VAR_ApiGetServicePasswd_dataJsonServiceName = json_object_get(DBOP_VAR_ApiGetServicePasswd_dataJsonAll, "service_username");

    if (!json_is_string(DBOP_VAR_ApiGetServicePasswd_dataJsonServiceName)) {
        dzlog_error("Invalid JSON data received. Expecting string types for 'service_username'");
        evhttp_send_reply(DBOP_VAR_ApiGetServicePasswd_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiGetServicePasswd_dataJsonAll);
        return;
    }
    dzlog_info("Validating JSON data, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);

    const char *DBOP_VAR_ApiGetServicePasswd_waitGetPassword = json_string_value(DBOP_VAR_ApiGetServicePasswd_dataJsonServiceName);

    dzlog_info("[%s] Executing database operation for ApiGetServicePasswd: %s", DBOP_VAR_ApiGetServicePasswd_requestId, DBOP_VAR_ApiGetServicePasswd_waitGetPassword);
    // 调用数据库函数 - 使用线程安全的随机数生成
    unsigned int seed = (unsigned int)pthread_self();
    int index = rand_r(&seed) % DBOP_GLV_actualDBPoolSize;  // 使用实际的连接池大小
    DB_CONNECTION *DBOP_VAR_ApiGetServicePasswd_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiGetServicePasswd_cfg, index);  // 从连接池中取出一个连接
    char* DBOP_VAR_ApiGetServicePasswd_returnPassword = DBOP_FUN_ExecuteGetServicePassword(DBOP_VAR_ApiGetServicePasswd_mysqlConnect, DBOP_VAR_ApiGetServicePasswd_waitGetPassword);
    // 创建返回体evbuffer实例
    struct evbuffer* DBOP_VAR_ApiGetServicePasswd_buffer = evbuffer_new();
    if (!DBOP_VAR_ApiGetServicePasswd_buffer) {
        dzlog_error("DBOP_VAR_ApiGetServicePasswd_buffer build exception, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);
        if (DBOP_VAR_ApiGetServicePasswd_returnPassword != NULL) {
            free(DBOP_VAR_ApiGetServicePasswd_returnPassword);
            DBOP_VAR_ApiGetServicePasswd_returnPassword = NULL;
        }
        evhttp_send_reply(DBOP_VAR_ApiGetServicePasswd_request, 400, "Bad Request", NULL);
        return;
    }
    if (evbuffer_add_printf(DBOP_VAR_ApiGetServicePasswd_buffer, "{\"ServiceName\":\"%s\", \"ServicePassword\":\"%s\"}", DBOP_VAR_ApiGetServicePasswd_waitGetPassword, DBOP_VAR_ApiGetServicePasswd_returnPassword) == -1) {
        dzlog_error("DBOP_VAR_ApiGetServicePasswd_buffer add failed, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);
        evbuffer_free(DBOP_VAR_ApiGetServicePasswd_buffer); 
        if (DBOP_VAR_ApiGetServicePasswd_returnPassword != NULL) {
            free(DBOP_VAR_ApiGetServicePasswd_returnPassword);
            DBOP_VAR_ApiGetServicePasswd_returnPassword = NULL;
        }
        evhttp_send_reply(DBOP_VAR_ApiGetServicePasswd_request, 400, "Bad Request", NULL);
        return;
    }
    evhttp_add_header(evhttp_request_get_output_headers(DBOP_VAR_ApiGetServicePasswd_request), "Content-Type", "application/json");

    // 发送HTTP响应
    evhttp_send_reply(DBOP_VAR_ApiGetServicePasswd_request, HTTP_OK, "OK", DBOP_VAR_ApiGetServicePasswd_buffer);
    evbuffer_free(DBOP_VAR_ApiGetServicePasswd_buffer); // 发送完毕后释放evbuffer
    if (DBOP_VAR_ApiGetServicePasswd_returnPassword != NULL) {
        free(DBOP_VAR_ApiGetServicePasswd_returnPassword);
        DBOP_VAR_ApiGetServicePasswd_returnPassword = NULL;
    }
    dzlog_info("Sending HTTP response, request id: %s", DBOP_VAR_ApiGetServicePasswd_requestId);
}


// ------------------------mysql根据servicename查询对应servicepassword逻辑结束----------------------------


// ------------------------mysql更新验证记录api逻辑开始----------------------------

// 更新验证记录的sql数据化输出
int DBOP_FUN_ExecuteUpdateVerificationRecord(DB_CONNECTION *DBOP_VAR_ExecuteUpdateVerificationRecord_connect, const char *DBOP_VAR_ExecuteUpdateVerificationRecord_projectId, const char *DBOP_VAR_ExecuteUpdateVerificationRecord_verificationRecord, const char *DBOP_VAR_ExecuteUpdateVerificationRecord_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateVerificationRecord is starting", DBOP_VAR_ExecuteUpdateVerificationRecord_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteUpdateVerificationRecord_noConstProjectId[256];
    char *DBOP_VAR_ExecuteUpdateVerificationRecord_noConstVerificationRecord = malloc(strlen(DBOP_VAR_ExecuteUpdateVerificationRecord_verificationRecord) + 1);
    
    if (DBOP_VAR_ExecuteUpdateVerificationRecord_noConstVerificationRecord == NULL) {
        dzlog_error("[req: %s] Memory allocation failed for verification record", DBOP_VAR_ExecuteUpdateVerificationRecord_requestId);
        return -1;
    }

    strncpy(DBOP_VAR_ExecuteUpdateVerificationRecord_noConstProjectId, DBOP_VAR_ExecuteUpdateVerificationRecord_projectId, sizeof(DBOP_VAR_ExecuteUpdateVerificationRecord_noConstProjectId) - 1);
    DBOP_VAR_ExecuteUpdateVerificationRecord_noConstProjectId[sizeof(DBOP_VAR_ExecuteUpdateVerificationRecord_noConstProjectId) - 1] = '\0';

    strcpy(DBOP_VAR_ExecuteUpdateVerificationRecord_noConstVerificationRecord, DBOP_VAR_ExecuteUpdateVerificationRecord_verificationRecord);

    MYSQL_BIND DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams[2];
    memset(DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams));

    // 绑定参数：verification_record, project_id
    DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams[0].buffer = DBOP_VAR_ExecuteUpdateVerificationRecord_noConstVerificationRecord;
    DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateVerificationRecord_noConstVerificationRecord);

    DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteUpdateVerificationRecord_noConstProjectId;
    DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateVerificationRecord_noConstProjectId);

    int result = 0;
    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateVerificationRecord_connect->stmt_update_verification_record, DBOP_VAR_ExecuteUpdateVerificationRecord_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateVerificationRecord_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateVerificationRecord_connect->stmt_update_verification_record));
        result = -1;
        goto cleanup;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateVerificationRecord_connect->stmt_update_verification_record)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateVerificationRecord_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateVerificationRecord_connect->stmt_update_verification_record));
        result = -1;
        goto cleanup;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateVerificationRecord_connect->stmt_update_verification_record);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No project found with project_id: %s", DBOP_VAR_ExecuteUpdateVerificationRecord_requestId, DBOP_VAR_ExecuteUpdateVerificationRecord_projectId);
        result = 1;
        goto cleanup;
    }

    dzlog_info("[req: %s] Successfully updated verification record for project: %s", DBOP_VAR_ExecuteUpdateVerificationRecord_requestId, DBOP_VAR_ExecuteUpdateVerificationRecord_projectId);

cleanup:
    free(DBOP_VAR_ExecuteUpdateVerificationRecord_noConstVerificationRecord);
    return result;
}

// 更新验证记录的API接口
void DBOP_FUN_ApiUpdateVerificationRecord(struct evhttp_request *DBOP_VAR_ApiUpdateVerificationRecord_request, void *DBOP_VAR_ApiUpdateVerificationRecord_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateVerificationRecord_cfg = (AppConfig *)DBOP_VAR_ApiUpdateVerificationRecord_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateVerificationRecord_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateVerificationRecord_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateVerificationRecord.", DBOP_VAR_ApiUpdateVerificationRecord_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateVerificationRecord_request, DBOP_VAR_ApiUpdateVerificationRecord_cfg, DBOP_VAR_ApiUpdateVerificationRecord_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateVerificationRecord_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateVerificationRecord_request, DBOP_VAR_ApiUpdateVerificationRecord_requestId);
    if (!DBOP_VAR_ApiUpdateVerificationRecord_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateVerificationRecord_dataJsonProjectId = json_object_get(DBOP_VAR_ApiUpdateVerificationRecord_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiUpdateVerificationRecord_dataJsonVerificationRecord = json_object_get(DBOP_VAR_ApiUpdateVerificationRecord_dataJsonAll, "update_verification_record");

    if (!json_is_string(DBOP_VAR_ApiUpdateVerificationRecord_dataJsonProjectId) ||
        !json_is_string(DBOP_VAR_ApiUpdateVerificationRecord_dataJsonVerificationRecord)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string types for 'project_id' and 'update_verification_record'", DBOP_VAR_ApiUpdateVerificationRecord_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateVerificationRecord_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateVerificationRecord_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateVerificationRecord_projectId = json_string_value(DBOP_VAR_ApiUpdateVerificationRecord_dataJsonProjectId);
    const char *DBOP_VAR_ApiUpdateVerificationRecord_verificationRecord = json_string_value(DBOP_VAR_ApiUpdateVerificationRecord_dataJsonVerificationRecord);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateVerificationRecord: projectId=%s", DBOP_VAR_ApiUpdateVerificationRecord_requestId, DBOP_VAR_ApiUpdateVerificationRecord_projectId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateVerificationRecord_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateVerificationRecord_cfg);
    int result = DBOP_FUN_ExecuteUpdateVerificationRecord(DBOP_VAR_ApiUpdateVerificationRecord_mysqlConnect, DBOP_VAR_ApiUpdateVerificationRecord_projectId, DBOP_VAR_ApiUpdateVerificationRecord_verificationRecord, DBOP_VAR_ApiUpdateVerificationRecord_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateVerificationRecord_request, result, DBOP_VAR_ApiUpdateVerificationRecord_requestId, "update verification record");

    json_decref(DBOP_VAR_ApiUpdateVerificationRecord_dataJsonAll);
}

// ------------------------mysql更新验证记录api逻辑结束----------------------------


// ------------------------mysql更新项目状态api逻辑开始----------------------------

// 更新项目状态的sql数据化输出
int DBOP_FUN_ExecuteUpdateProjectStatus(DB_CONNECTION *DBOP_VAR_ExecuteUpdateProjectStatus_connect, const char *DBOP_VAR_ExecuteUpdateProjectStatus_projectId, int DBOP_VAR_ExecuteUpdateProjectStatus_projectStatus, const char *DBOP_VAR_ExecuteUpdateProjectStatus_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateProjectStatus is starting", DBOP_VAR_ExecuteUpdateProjectStatus_requestId);

    // 保护原始参数不被修改，复制项目ID
    char DBOP_VAR_ExecuteUpdateProjectStatus_noConstProjectId[256];
    strncpy(DBOP_VAR_ExecuteUpdateProjectStatus_noConstProjectId, DBOP_VAR_ExecuteUpdateProjectStatus_projectId, sizeof(DBOP_VAR_ExecuteUpdateProjectStatus_noConstProjectId) - 1);
    DBOP_VAR_ExecuteUpdateProjectStatus_noConstProjectId[sizeof(DBOP_VAR_ExecuteUpdateProjectStatus_noConstProjectId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteUpdateProjectStatus_bindParams[2];
    memset(DBOP_VAR_ExecuteUpdateProjectStatus_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateProjectStatus_bindParams));

    // 绑定参数：project_status, project_id
    DBOP_VAR_ExecuteUpdateProjectStatus_bindParams[0].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteUpdateProjectStatus_bindParams[0].buffer = &DBOP_VAR_ExecuteUpdateProjectStatus_projectStatus;
    DBOP_VAR_ExecuteUpdateProjectStatus_bindParams[0].buffer_length = sizeof(DBOP_VAR_ExecuteUpdateProjectStatus_projectStatus);

    DBOP_VAR_ExecuteUpdateProjectStatus_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectStatus_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteUpdateProjectStatus_noConstProjectId;
    DBOP_VAR_ExecuteUpdateProjectStatus_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateProjectStatus_noConstProjectId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateProjectStatus_connect->stmt_update_project_status, DBOP_VAR_ExecuteUpdateProjectStatus_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateProjectStatus_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectStatus_connect->stmt_update_project_status));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateProjectStatus_connect->stmt_update_project_status)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateProjectStatus_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectStatus_connect->stmt_update_project_status));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateProjectStatus_connect->stmt_update_project_status);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No project found with project_id: %s", DBOP_VAR_ExecuteUpdateProjectStatus_requestId, DBOP_VAR_ExecuteUpdateProjectStatus_projectId);
        return 1;
    }

    dzlog_info("[req: %s] Successfully updated project status for project: %s to status: %d", DBOP_VAR_ExecuteUpdateProjectStatus_requestId, DBOP_VAR_ExecuteUpdateProjectStatus_projectId, DBOP_VAR_ExecuteUpdateProjectStatus_projectStatus);
    return 0;
}

// 更新项目状态的API接口
void DBOP_FUN_ApiUpdateProjectStatus(struct evhttp_request *DBOP_VAR_ApiUpdateProjectStatus_request, void *DBOP_VAR_ApiUpdateProjectStatus_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateProjectStatus_cfg = (AppConfig *)DBOP_VAR_ApiUpdateProjectStatus_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateProjectStatus_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateProjectStatus_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateProjectStatus.", DBOP_VAR_ApiUpdateProjectStatus_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateProjectStatus_request, DBOP_VAR_ApiUpdateProjectStatus_cfg, DBOP_VAR_ApiUpdateProjectStatus_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateProjectStatus_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateProjectStatus_request, DBOP_VAR_ApiUpdateProjectStatus_requestId);
    if (!DBOP_VAR_ApiUpdateProjectStatus_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateProjectStatus_dataJsonProjectId = json_object_get(DBOP_VAR_ApiUpdateProjectStatus_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiUpdateProjectStatus_dataJsonProjectStatus = json_object_get(DBOP_VAR_ApiUpdateProjectStatus_dataJsonAll, "projest_status");

    if (!json_is_string(DBOP_VAR_ApiUpdateProjectStatus_dataJsonProjectId) ||
        !json_is_integer(DBOP_VAR_ApiUpdateProjectStatus_dataJsonProjectStatus)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'project_id' and integer type for 'projest_status'", DBOP_VAR_ApiUpdateProjectStatus_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateProjectStatus_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateProjectStatus_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateProjectStatus_projectId = json_string_value(DBOP_VAR_ApiUpdateProjectStatus_dataJsonProjectId);
    int DBOP_VAR_ApiUpdateProjectStatus_projectStatus = json_integer_value(DBOP_VAR_ApiUpdateProjectStatus_dataJsonProjectStatus);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateProjectStatus: projectId=%s, projectStatus=%d", DBOP_VAR_ApiUpdateProjectStatus_requestId, DBOP_VAR_ApiUpdateProjectStatus_projectId, DBOP_VAR_ApiUpdateProjectStatus_projectStatus);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateProjectStatus_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateProjectStatus_cfg);
    int result = DBOP_FUN_ExecuteUpdateProjectStatus(DBOP_VAR_ApiUpdateProjectStatus_mysqlConnect, DBOP_VAR_ApiUpdateProjectStatus_projectId, DBOP_VAR_ApiUpdateProjectStatus_projectStatus, DBOP_VAR_ApiUpdateProjectStatus_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateProjectStatus_request, result, DBOP_VAR_ApiUpdateProjectStatus_requestId, "update project status");

    json_decref(DBOP_VAR_ApiUpdateProjectStatus_dataJsonAll);
}

// ------------------------mysql更新项目状态api逻辑结束----------------------------


// ------------------------mysql更新病情记录api逻辑开始----------------------------

// 更新病情记录的sql数据化输出
int DBOP_FUN_ExecuteUpdateProjectPathography(DB_CONNECTION *DBOP_VAR_ExecuteUpdateProjectPathography_connect, const char *DBOP_VAR_ExecuteUpdateProjectPathography_projectId, const char *DBOP_VAR_ExecuteUpdateProjectPathography_pathography, const char *DBOP_VAR_ExecuteUpdateProjectPathography_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateProjectPathography is starting", DBOP_VAR_ExecuteUpdateProjectPathography_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteUpdateProjectPathography_noConstProjectId[256];
    char *DBOP_VAR_ExecuteUpdateProjectPathography_noConstPathography = malloc(strlen(DBOP_VAR_ExecuteUpdateProjectPathography_pathography) + 1);
    
    if (DBOP_VAR_ExecuteUpdateProjectPathography_noConstPathography == NULL) {
        dzlog_error("[req: %s] Memory allocation failed for pathography", DBOP_VAR_ExecuteUpdateProjectPathography_requestId);
        return -1;
    }

    strncpy(DBOP_VAR_ExecuteUpdateProjectPathography_noConstProjectId, DBOP_VAR_ExecuteUpdateProjectPathography_projectId, sizeof(DBOP_VAR_ExecuteUpdateProjectPathography_noConstProjectId) - 1);
    DBOP_VAR_ExecuteUpdateProjectPathography_noConstProjectId[sizeof(DBOP_VAR_ExecuteUpdateProjectPathography_noConstProjectId) - 1] = '\0';

    strcpy(DBOP_VAR_ExecuteUpdateProjectPathography_noConstPathography, DBOP_VAR_ExecuteUpdateProjectPathography_pathography);

    MYSQL_BIND DBOP_VAR_ExecuteUpdateProjectPathography_bindParams[2];
    memset(DBOP_VAR_ExecuteUpdateProjectPathography_bindParams, 0, sizeof(DBOP_VAR_ExecuteUpdateProjectPathography_bindParams));

    // 绑定参数：pathography, project_id
    DBOP_VAR_ExecuteUpdateProjectPathography_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectPathography_bindParams[0].buffer = DBOP_VAR_ExecuteUpdateProjectPathography_noConstPathography;
    DBOP_VAR_ExecuteUpdateProjectPathography_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateProjectPathography_noConstPathography);

    DBOP_VAR_ExecuteUpdateProjectPathography_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectPathography_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteUpdateProjectPathography_noConstProjectId;
    DBOP_VAR_ExecuteUpdateProjectPathography_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateProjectPathography_noConstProjectId);

    int result = 0;
    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateProjectPathography_connect->stmt_update_project_pathography, DBOP_VAR_ExecuteUpdateProjectPathography_bindParams)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateProjectPathography_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectPathography_connect->stmt_update_project_pathography));
        result = -1;
        goto cleanup;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateProjectPathography_connect->stmt_update_project_pathography)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateProjectPathography_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectPathography_connect->stmt_update_project_pathography));
        result = -1;
        goto cleanup;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteUpdateProjectPathography_connect->stmt_update_project_pathography);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No project found with project_id: %s", DBOP_VAR_ExecuteUpdateProjectPathography_requestId, DBOP_VAR_ExecuteUpdateProjectPathography_projectId);
        result = 1;
        goto cleanup;
    }

    dzlog_info("[req: %s] Successfully updated pathography for project: %s", DBOP_VAR_ExecuteUpdateProjectPathography_requestId, DBOP_VAR_ExecuteUpdateProjectPathography_projectId);

cleanup:
    free(DBOP_VAR_ExecuteUpdateProjectPathography_noConstPathography);
    return result;
}

// 更新病情记录的API接口
void DBOP_FUN_ApiUpdateProjectPathography(struct evhttp_request *DBOP_VAR_ApiUpdateProjectPathography_request, void *DBOP_VAR_ApiUpdateProjectPathography_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateProjectPathography_cfg = (AppConfig *)DBOP_VAR_ApiUpdateProjectPathography_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateProjectPathography_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateProjectPathography_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateProjectPathography.", DBOP_VAR_ApiUpdateProjectPathography_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateProjectPathography_request, DBOP_VAR_ApiUpdateProjectPathography_cfg, DBOP_VAR_ApiUpdateProjectPathography_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateProjectPathography_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateProjectPathography_request, DBOP_VAR_ApiUpdateProjectPathography_requestId);
    if (!DBOP_VAR_ApiUpdateProjectPathography_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateProjectPathography_dataJsonProjectId = json_object_get(DBOP_VAR_ApiUpdateProjectPathography_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiUpdateProjectPathography_dataJsonPathography = json_object_get(DBOP_VAR_ApiUpdateProjectPathography_dataJsonAll, "pathography");

    if (!json_is_string(DBOP_VAR_ApiUpdateProjectPathography_dataJsonProjectId) ||
        !json_is_string(DBOP_VAR_ApiUpdateProjectPathography_dataJsonPathography)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string types for 'project_id' and 'pathography'", DBOP_VAR_ApiUpdateProjectPathography_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateProjectPathography_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateProjectPathography_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateProjectPathography_projectId = json_string_value(DBOP_VAR_ApiUpdateProjectPathography_dataJsonProjectId);
    const char *DBOP_VAR_ApiUpdateProjectPathography_pathography = json_string_value(DBOP_VAR_ApiUpdateProjectPathography_dataJsonPathography);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateProjectPathography: projectId=%s", DBOP_VAR_ApiUpdateProjectPathography_requestId, DBOP_VAR_ApiUpdateProjectPathography_projectId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateProjectPathography_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateProjectPathography_cfg);
    int result = DBOP_FUN_ExecuteUpdateProjectPathography(DBOP_VAR_ApiUpdateProjectPathography_mysqlConnect, DBOP_VAR_ApiUpdateProjectPathography_projectId, DBOP_VAR_ApiUpdateProjectPathography_pathography, DBOP_VAR_ApiUpdateProjectPathography_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateProjectPathography_request, result, DBOP_VAR_ApiUpdateProjectPathography_requestId, "update project pathography");

    json_decref(DBOP_VAR_ApiUpdateProjectPathography_dataJsonAll);
}

// ------------------------mysql更新病情记录api逻辑结束----------------------------


// ------------------------mysql更新项目负责志愿者ID api逻辑开始----------------------------

// 更新项目负责志愿者ID的sql数据化输出
int DBOP_FUN_ExecuteUpdateProjectVolunteer(DB_CONNECTION *DBOP_VAR_ExecuteUpdateProjectVolunteer_connect, const char *DBOP_VAR_ExecuteUpdateProjectVolunteer_projectId, const char *DBOP_VAR_ExecuteUpdateProjectVolunteer_volunteerId, const char *DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteUpdateProjectVolunteer is starting", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId[256];
    char DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstVolunteerId[256];

    strncpy(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId, DBOP_VAR_ExecuteUpdateProjectVolunteer_projectId, sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId) - 1);
    DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId[sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstVolunteerId, DBOP_VAR_ExecuteUpdateProjectVolunteer_volunteerId, sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstVolunteerId) - 1);
    DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstVolunteerId[sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstVolunteerId) - 1] = '\0';

    // 开始事务
    if (mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 0)) {
        dzlog_error("[req: %s] Failed to start transaction: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, mysql_error(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql));
        return -1;
    }

    // 第一步：查询当前志愿者信息
    MYSQL_BIND DBOP_VAR_ExecuteUpdateProjectVolunteer_queryBind[1];
    memset(DBOP_VAR_ExecuteUpdateProjectVolunteer_queryBind, 0, sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_queryBind));

    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryBind[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryBind[0].buffer = (char *)DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryBind[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info, DBOP_VAR_ExecuteUpdateProjectVolunteer_queryBind)) {
        dzlog_error("[req: %s] Failed to bind query param: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info));
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info)) {
        dzlog_error("[req: %s] Failed to execute query statement: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info));
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return -1;
    }

    // 绑定查询结果
    MYSQL_BIND DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[2];
    memset(DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult, 0, sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult));
    
    char DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteer[256];
    char DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteers[4096];
    unsigned long DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerLength;
    unsigned long DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersLength;
    my_bool DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerIsNull;
    my_bool DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersIsNull;

    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[0].buffer = DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteer;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[0].buffer_length = sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteer);
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[0].length = &DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerLength;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[0].is_null = &DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerIsNull;

    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[1].buffer = DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteers;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[1].buffer_length = sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteers);
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[1].length = &DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersLength;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult[1].is_null = &DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersIsNull;

    if (mysql_stmt_bind_result(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info, DBOP_VAR_ExecuteUpdateProjectVolunteer_queryResult)) {
        dzlog_error("[req: %s] Failed to bind query result: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info));
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return -1;
    }

    // 获取查询结果
    int fetchResult = mysql_stmt_fetch(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info);
    if (fetchResult == MYSQL_NO_DATA) {
        dzlog_warn("[req: %s] No project found with project_id: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, DBOP_VAR_ExecuteUpdateProjectVolunteer_projectId);
        mysql_stmt_free_result(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info);
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return 1;
    } else if (fetchResult != 0) {
        dzlog_error("[req: %s] Failed to fetch query result: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info));
        mysql_stmt_free_result(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info);
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return -1;
    }

    mysql_stmt_free_result(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_get_project_volunteer_info);

    // 确保字符串正确终止
    if (!DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerIsNull) {
        DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteer[DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerLength] = '\0';
    }
    if (!DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersIsNull) {
        DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteers[DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersLength] = '\0';
    }

    // 第二步：构建新的previous_volunteers列表
    char *DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers = NULL;
    
    if (DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerIsNull || DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerLength == 0) {
        // 当前没有志愿者，保持previous_volunteers不变
        if (DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersIsNull) {
            DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers = strdup("[]");
        } else {
            DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers = strdup(DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteers);
        }
    } else {
        // 有当前志愿者，需要添加到previous_volunteers列表末尾
        json_error_t error;
        json_t *previousArray;
        
        if (DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersIsNull || DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteersLength == 0) {
            // 创建新的数组
            previousArray = json_array();
        } else {
            // 解析现有的JSON数组
            previousArray = json_loads(DBOP_VAR_ExecuteUpdateProjectVolunteer_previousVolunteers, 0, &error);
            if (!previousArray || !json_is_array(previousArray)) {
                dzlog_warn("[req: %s] Invalid previous_volunteers JSON, creating new array", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId);
                if (previousArray) json_decref(previousArray);
                previousArray = json_array();
            }
        }
        
        // 添加当前志愿者到数组末尾
        json_t *currentVolunteerJson = json_string(DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteer);
        json_array_append_new(previousArray, currentVolunteerJson);
        
        // 将数组转换为字符串
        DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers = json_dumps(previousArray, JSON_COMPACT);
        json_decref(previousArray);
    }

    if (!DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers) {
        dzlog_error("[req: %s] Failed to create new previous_volunteers string", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId);
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return -1;
    }

    // 第三步：更新project表
    MYSQL_BIND DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[3];
    memset(DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind, 0, sizeof(DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind));

    // 绑定参数：current_volunteer, previous_volunteers, project_id
    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[0].buffer = (char *)DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstVolunteerId;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[0].buffer_length = strlen(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstVolunteerId);

    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[1].buffer = DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[1].buffer_length = strlen(DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers);

    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[2].buffer = (char *)DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId;
    DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind[2].buffer_length = strlen(DBOP_VAR_ExecuteUpdateProjectVolunteer_noConstProjectId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_update_project_volunteer, DBOP_VAR_ExecuteUpdateProjectVolunteer_updateBind)) {
        dzlog_error("[req: %s] Failed to bind update param: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_update_project_volunteer));
        free(DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers);
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_update_project_volunteer)) {
        dzlog_error("[req: %s] Failed to execute update statement: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, mysql_stmt_error(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->stmt_update_project_volunteer));
        free(DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers);
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return -1;
    }

    // 提交事务
    if (mysql_commit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql)) {
        dzlog_error("[req: %s] Failed to commit transaction: %s", DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, mysql_error(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql));
        free(DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers);
        mysql_rollback(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql);
        mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);
        return -1;
    }

    // 恢复自动提交
    mysql_autocommit(DBOP_VAR_ExecuteUpdateProjectVolunteer_connect->mysql, 1);

    dzlog_info("[req: %s] Successfully updated volunteer for project: %s from %s to %s", 
               DBOP_VAR_ExecuteUpdateProjectVolunteer_requestId, 
               DBOP_VAR_ExecuteUpdateProjectVolunteer_projectId, 
               DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteerIsNull ? "NULL" : DBOP_VAR_ExecuteUpdateProjectVolunteer_currentVolunteer, 
               DBOP_VAR_ExecuteUpdateProjectVolunteer_volunteerId);

    free(DBOP_VAR_ExecuteUpdateProjectVolunteer_newPreviousVolunteers);
    return 0;
}

// 更新项目负责志愿者ID的API接口
void DBOP_FUN_ApiUpdateProjectVolunteer(struct evhttp_request *DBOP_VAR_ApiUpdateProjectVolunteer_request, void *DBOP_VAR_ApiUpdateProjectVolunteer_voidCfg) {
    AppConfig *DBOP_VAR_ApiUpdateProjectVolunteer_cfg = (AppConfig *)DBOP_VAR_ApiUpdateProjectVolunteer_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiUpdateProjectVolunteer_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiUpdateProjectVolunteer_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiUpdateProjectVolunteer.", DBOP_VAR_ApiUpdateProjectVolunteer_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiUpdateProjectVolunteer_request, DBOP_VAR_ApiUpdateProjectVolunteer_cfg, DBOP_VAR_ApiUpdateProjectVolunteer_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiUpdateProjectVolunteer_request, DBOP_VAR_ApiUpdateProjectVolunteer_requestId);
    if (!DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonProjectId = json_object_get(DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonVolunteerId = json_object_get(DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonAll, "volunteer_id");

    if (!json_is_string(DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonProjectId) ||
        !json_is_string(DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonVolunteerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string types for 'project_id' and 'volunteer_id'", DBOP_VAR_ApiUpdateProjectVolunteer_requestId);
        evhttp_send_reply(DBOP_VAR_ApiUpdateProjectVolunteer_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiUpdateProjectVolunteer_projectId = json_string_value(DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonProjectId);
    const char *DBOP_VAR_ApiUpdateProjectVolunteer_volunteerId = json_string_value(DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonVolunteerId);

    dzlog_info("[req: %s] Executing database operation for ApiUpdateProjectVolunteer: projectId=%s, volunteerId=%s", DBOP_VAR_ApiUpdateProjectVolunteer_requestId, DBOP_VAR_ApiUpdateProjectVolunteer_projectId, DBOP_VAR_ApiUpdateProjectVolunteer_volunteerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiUpdateProjectVolunteer_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiUpdateProjectVolunteer_cfg);
    int result = DBOP_FUN_ExecuteUpdateProjectVolunteer(DBOP_VAR_ApiUpdateProjectVolunteer_mysqlConnect, DBOP_VAR_ApiUpdateProjectVolunteer_projectId, DBOP_VAR_ApiUpdateProjectVolunteer_volunteerId, DBOP_VAR_ApiUpdateProjectVolunteer_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiUpdateProjectVolunteer_request, result, DBOP_VAR_ApiUpdateProjectVolunteer_requestId, "update project volunteer");

    json_decref(DBOP_VAR_ApiUpdateProjectVolunteer_dataJsonAll);
}

// ------------------------mysql更新项目负责志愿者ID api逻辑结束----------------------------


// ------------------------mysql新增捐款记录 api逻辑开始----------------------------

// 新增捐款记录的sql数据化输出
int DBOP_FUN_ExecuteMakeDonation(DB_CONNECTION *DBOP_VAR_ExecuteMakeDonation_connect, const char *DBOP_VAR_ExecuteMakeDonation_ledgerId, const char *DBOP_VAR_ExecuteMakeDonation_projectId, const char *DBOP_VAR_ExecuteMakeDonation_userId, const char *DBOP_VAR_ExecuteMakeDonation_suffererUserId, const char *DBOP_VAR_ExecuteMakeDonation_donorUserName, const char *DBOP_VAR_ExecuteMakeDonation_suffererRealName, const char *DBOP_VAR_ExecuteMakeDonation_suffererUserName, int DBOP_VAR_ExecuteMakeDonation_amount, const char *DBOP_VAR_ExecuteMakeDonation_note, int DBOP_VAR_ExecuteMakeDonation_paymentMethod, const char *DBOP_VAR_ExecuteMakeDonation_methodId, const char *DBOP_VAR_ExecuteMakeDonation_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMakeDonation is starting", DBOP_VAR_ExecuteMakeDonation_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMakeDonation_noConstLedgerId[256];
    char DBOP_VAR_ExecuteMakeDonation_noConstProjectId[256];
    char DBOP_VAR_ExecuteMakeDonation_noConstUserId[256];
    char DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserId[256];
    char DBOP_VAR_ExecuteMakeDonation_noConstDonorUserName[256];
    char DBOP_VAR_ExecuteMakeDonation_noConstSuffererRealName[256];
    char DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserName[256];
    char DBOP_VAR_ExecuteMakeDonation_noConstNote[512];
    char DBOP_VAR_ExecuteMakeDonation_noConstMethodId[256];

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstLedgerId, DBOP_VAR_ExecuteMakeDonation_ledgerId, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstLedgerId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstProjectId, DBOP_VAR_ExecuteMakeDonation_projectId, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstProjectId) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstProjectId[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstProjectId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstUserId, DBOP_VAR_ExecuteMakeDonation_userId, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstUserId) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstUserId[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserId, DBOP_VAR_ExecuteMakeDonation_suffererUserId, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserId) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserId[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstDonorUserName, DBOP_VAR_ExecuteMakeDonation_donorUserName, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstDonorUserName) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstDonorUserName[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstDonorUserName) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstSuffererRealName, DBOP_VAR_ExecuteMakeDonation_suffererRealName, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstSuffererRealName) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstSuffererRealName[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstSuffererRealName) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserName, DBOP_VAR_ExecuteMakeDonation_suffererUserName, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserName) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserName[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserName) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstNote, DBOP_VAR_ExecuteMakeDonation_note, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstNote) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstNote[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstNote) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteMakeDonation_noConstMethodId, DBOP_VAR_ExecuteMakeDonation_methodId, sizeof(DBOP_VAR_ExecuteMakeDonation_noConstMethodId) - 1);
    DBOP_VAR_ExecuteMakeDonation_noConstMethodId[sizeof(DBOP_VAR_ExecuteMakeDonation_noConstMethodId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMakeDonation_bindParams[11];
    memset(DBOP_VAR_ExecuteMakeDonation_bindParams, 0, sizeof(DBOP_VAR_ExecuteMakeDonation_bindParams));

    // 绑定参数: ledger_id, project_id, user_id, sufferer_user_id, donor_user_name, sufferer_real_name, sufferer_user_name, amount, note, payment_method, method_id
    DBOP_VAR_ExecuteMakeDonation_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[0].buffer = DBOP_VAR_ExecuteMakeDonation_noConstLedgerId;
    DBOP_VAR_ExecuteMakeDonation_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstLedgerId);

    DBOP_VAR_ExecuteMakeDonation_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[1].buffer = DBOP_VAR_ExecuteMakeDonation_noConstProjectId;
    DBOP_VAR_ExecuteMakeDonation_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstProjectId);

    DBOP_VAR_ExecuteMakeDonation_bindParams[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[2].buffer = DBOP_VAR_ExecuteMakeDonation_noConstUserId;
    DBOP_VAR_ExecuteMakeDonation_bindParams[2].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstUserId);

    DBOP_VAR_ExecuteMakeDonation_bindParams[3].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[3].buffer = DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserId;
    DBOP_VAR_ExecuteMakeDonation_bindParams[3].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserId);

    DBOP_VAR_ExecuteMakeDonation_bindParams[4].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[4].buffer = DBOP_VAR_ExecuteMakeDonation_noConstDonorUserName;
    DBOP_VAR_ExecuteMakeDonation_bindParams[4].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstDonorUserName);

    DBOP_VAR_ExecuteMakeDonation_bindParams[5].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[5].buffer = DBOP_VAR_ExecuteMakeDonation_noConstSuffererRealName;
    DBOP_VAR_ExecuteMakeDonation_bindParams[5].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstSuffererRealName);

    DBOP_VAR_ExecuteMakeDonation_bindParams[6].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[6].buffer = DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserName;
    DBOP_VAR_ExecuteMakeDonation_bindParams[6].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstSuffererUserName);

    DBOP_VAR_ExecuteMakeDonation_bindParams[7].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteMakeDonation_bindParams[7].buffer = &DBOP_VAR_ExecuteMakeDonation_amount;
    DBOP_VAR_ExecuteMakeDonation_bindParams[7].buffer_length = sizeof(DBOP_VAR_ExecuteMakeDonation_amount);

    DBOP_VAR_ExecuteMakeDonation_bindParams[8].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[8].buffer = DBOP_VAR_ExecuteMakeDonation_noConstNote;
    DBOP_VAR_ExecuteMakeDonation_bindParams[8].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstNote);

    DBOP_VAR_ExecuteMakeDonation_bindParams[9].buffer_type = MYSQL_TYPE_TINY;
    DBOP_VAR_ExecuteMakeDonation_bindParams[9].buffer = &DBOP_VAR_ExecuteMakeDonation_paymentMethod;
    DBOP_VAR_ExecuteMakeDonation_bindParams[9].buffer_length = sizeof(DBOP_VAR_ExecuteMakeDonation_paymentMethod);

    DBOP_VAR_ExecuteMakeDonation_bindParams[10].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMakeDonation_bindParams[10].buffer = DBOP_VAR_ExecuteMakeDonation_noConstMethodId;
    DBOP_VAR_ExecuteMakeDonation_bindParams[10].buffer_length = strlen(DBOP_VAR_ExecuteMakeDonation_noConstMethodId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMakeDonation_connect->stmt_make_donation, DBOP_VAR_ExecuteMakeDonation_bindParams)) {
        dzlog_error("[req: %s] Failed to bind donation param: %s", DBOP_VAR_ExecuteMakeDonation_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMakeDonation_connect->stmt_make_donation));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMakeDonation_connect->stmt_make_donation)) {
        dzlog_error("[req: %s] Failed to execute donation statement: %s", DBOP_VAR_ExecuteMakeDonation_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMakeDonation_connect->stmt_make_donation));
        return -1;
    }

    dzlog_info("[req: %s] Successfully executed donation insert for ledger: %s, project: %s, amount: %d", DBOP_VAR_ExecuteMakeDonation_requestId, DBOP_VAR_ExecuteMakeDonation_ledgerId, DBOP_VAR_ExecuteMakeDonation_projectId, DBOP_VAR_ExecuteMakeDonation_amount);
    return 0;
}

// 新增捐款记录的API接口
void DBOP_FUN_ApiMakeDonation(struct evhttp_request *DBOP_VAR_ApiMakeDonation_request, void *DBOP_VAR_ApiMakeDonation_voidCfg) {
    AppConfig *DBOP_VAR_ApiMakeDonation_cfg = (AppConfig *)DBOP_VAR_ApiMakeDonation_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMakeDonation_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMakeDonation_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMakeDonation.", DBOP_VAR_ApiMakeDonation_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMakeDonation_request, DBOP_VAR_ApiMakeDonation_cfg, DBOP_VAR_ApiMakeDonation_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMakeDonation_request, DBOP_VAR_ApiMakeDonation_requestId);
    if (!DBOP_VAR_ApiMakeDonation_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "ledger_id");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonProjectId = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonUserId = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonSuffererUserId = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "sufferer_user_id");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonDonorUserName = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "donor_user_name");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonSuffererRealName = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "sufferer_real_name");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonSuffererUserName = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "sufferer_user_name");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonAmount = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "amount");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonNote = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "note");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonPaymentMethod = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "payment_method");
    json_t *DBOP_VAR_ApiMakeDonation_dataJsonMethodId = json_object_get(DBOP_VAR_ApiMakeDonation_dataJsonAll, "method_id");

    if (!json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonLedgerId) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonProjectId) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonSuffererUserId) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonDonorUserName) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonSuffererRealName) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonSuffererUserName) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonAmount) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonNote) ||
        !json_is_integer(DBOP_VAR_ApiMakeDonation_dataJsonPaymentMethod) ||
        !json_is_string(DBOP_VAR_ApiMakeDonation_dataJsonMethodId)) {
        dzlog_error("[req: %s] Invalid JSON data received. All required fields must be present and of correct types", DBOP_VAR_ApiMakeDonation_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMakeDonation_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMakeDonation_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMakeDonation_ledgerId = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonLedgerId);
    const char *DBOP_VAR_ApiMakeDonation_projectId = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonProjectId);
    const char *DBOP_VAR_ApiMakeDonation_userId = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonUserId);
    const char *DBOP_VAR_ApiMakeDonation_suffererUserId = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonSuffererUserId);
    const char *DBOP_VAR_ApiMakeDonation_donorUserName = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonDonorUserName);
    const char *DBOP_VAR_ApiMakeDonation_suffererRealName = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonSuffererRealName);
    const char *DBOP_VAR_ApiMakeDonation_suffererUserName = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonSuffererUserName);
    const char *DBOP_VAR_ApiMakeDonation_amountStr = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonAmount);
    const char *DBOP_VAR_ApiMakeDonation_note = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonNote);
    int DBOP_VAR_ApiMakeDonation_paymentMethod = json_integer_value(DBOP_VAR_ApiMakeDonation_dataJsonPaymentMethod);
    const char *DBOP_VAR_ApiMakeDonation_methodId = json_string_value(DBOP_VAR_ApiMakeDonation_dataJsonMethodId);

    // 将金额字符串转换为整数
    int DBOP_VAR_ApiMakeDonation_amount = atoi(DBOP_VAR_ApiMakeDonation_amountStr);
    if (DBOP_VAR_ApiMakeDonation_amount <= 0) {
        dzlog_error("[req: %s] Invalid amount value: %s", DBOP_VAR_ApiMakeDonation_requestId, DBOP_VAR_ApiMakeDonation_amountStr);
        evhttp_send_reply(DBOP_VAR_ApiMakeDonation_request, 400, "Bad Request - Invalid amount", NULL);
        json_decref(DBOP_VAR_ApiMakeDonation_dataJsonAll);
        return;
    }

    dzlog_info("[req: %s] Executing database operation for ApiMakeDonation: ledgerId=%s, projectId=%s, userId=%s, amount=%d", DBOP_VAR_ApiMakeDonation_requestId, DBOP_VAR_ApiMakeDonation_ledgerId, DBOP_VAR_ApiMakeDonation_projectId, DBOP_VAR_ApiMakeDonation_userId, DBOP_VAR_ApiMakeDonation_amount);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMakeDonation_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMakeDonation_cfg);
    int result = DBOP_FUN_ExecuteMakeDonation(DBOP_VAR_ApiMakeDonation_mysqlConnect, DBOP_VAR_ApiMakeDonation_ledgerId, DBOP_VAR_ApiMakeDonation_projectId, DBOP_VAR_ApiMakeDonation_userId, DBOP_VAR_ApiMakeDonation_suffererUserId, DBOP_VAR_ApiMakeDonation_donorUserName, DBOP_VAR_ApiMakeDonation_suffererRealName, DBOP_VAR_ApiMakeDonation_suffererUserName, DBOP_VAR_ApiMakeDonation_amount, DBOP_VAR_ApiMakeDonation_note, DBOP_VAR_ApiMakeDonation_paymentMethod, DBOP_VAR_ApiMakeDonation_methodId, DBOP_VAR_ApiMakeDonation_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMakeDonation_request, result, DBOP_VAR_ApiMakeDonation_requestId, "make donation");

    json_decref(DBOP_VAR_ApiMakeDonation_dataJsonAll);
}

// ------------------------mysql新增捐款记录 api逻辑结束----------------------------


// ------------------------mysql标记支付失败api逻辑开始----------------------------

// 标记支付失败的sql数据化输出
int DBOP_FUN_ExecuteMarkPaymentFailed(DB_CONNECTION *DBOP_VAR_ExecuteMarkPaymentFailed_connect, const char *DBOP_VAR_ExecuteMarkPaymentFailed_ledgerId, const char *DBOP_VAR_ExecuteMarkPaymentFailed_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkPaymentFailed is starting", DBOP_VAR_ExecuteMarkPaymentFailed_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkPaymentFailed_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkPaymentFailed_noConstLedgerId, DBOP_VAR_ExecuteMarkPaymentFailed_ledgerId, sizeof(DBOP_VAR_ExecuteMarkPaymentFailed_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkPaymentFailed_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkPaymentFailed_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkPaymentFailed_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkPaymentFailed_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkPaymentFailed_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkPaymentFailed_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkPaymentFailed_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkPaymentFailed_noConstLedgerId;
    DBOP_VAR_ExecuteMarkPaymentFailed_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkPaymentFailed_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkPaymentFailed_connect->stmt_set_status_payment_failed, DBOP_VAR_ExecuteMarkPaymentFailed_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark payment failed param: %s", DBOP_VAR_ExecuteMarkPaymentFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentFailed_connect->stmt_set_status_payment_failed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkPaymentFailed_connect->stmt_set_status_payment_failed)) {
        dzlog_error("[req: %s] Failed to execute mark payment failed statement: %s", DBOP_VAR_ExecuteMarkPaymentFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentFailed_connect->stmt_set_status_payment_failed));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkPaymentFailed_connect->stmt_set_status_payment_failed);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No donation record found with ledger_id: %s or transaction_type is not donation", DBOP_VAR_ExecuteMarkPaymentFailed_requestId, DBOP_VAR_ExecuteMarkPaymentFailed_ledgerId);
        return 1; // 表示记录不存在或不是捐赠类型
    }

    dzlog_info("[req: %s] Successfully marked payment as failed for ledger: %s", DBOP_VAR_ExecuteMarkPaymentFailed_requestId, DBOP_VAR_ExecuteMarkPaymentFailed_ledgerId);
    return 0;
}

// 标记支付失败的API接口
void DBOP_FUN_ApiMarkPaymentFailed(struct evhttp_request *DBOP_VAR_ApiMarkPaymentFailed_request, void *DBOP_VAR_ApiMarkPaymentFailed_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkPaymentFailed_cfg = (AppConfig *)DBOP_VAR_ApiMarkPaymentFailed_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkPaymentFailed_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkPaymentFailed_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkPaymentFailed.", DBOP_VAR_ApiMarkPaymentFailed_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkPaymentFailed_request, DBOP_VAR_ApiMarkPaymentFailed_cfg, DBOP_VAR_ApiMarkPaymentFailed_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkPaymentFailed_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkPaymentFailed_request, DBOP_VAR_ApiMarkPaymentFailed_requestId);
    if (!DBOP_VAR_ApiMarkPaymentFailed_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkPaymentFailed_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkPaymentFailed_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkPaymentFailed_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkPaymentFailed_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkPaymentFailed_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkPaymentFailed_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkPaymentFailed_ledgerId = json_string_value(DBOP_VAR_ApiMarkPaymentFailed_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkPaymentFailed: ledgerId=%s", DBOP_VAR_ApiMarkPaymentFailed_requestId, DBOP_VAR_ApiMarkPaymentFailed_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkPaymentFailed_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkPaymentFailed_cfg);
    int result = DBOP_FUN_ExecuteMarkPaymentFailed(DBOP_VAR_ApiMarkPaymentFailed_mysqlConnect, DBOP_VAR_ApiMarkPaymentFailed_ledgerId, DBOP_VAR_ApiMarkPaymentFailed_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkPaymentFailed_request, result, DBOP_VAR_ApiMarkPaymentFailed_requestId, "mark payment failed");

    json_decref(DBOP_VAR_ApiMarkPaymentFailed_dataJsonAll);
}

// ------------------------mysql标记支付失败api逻辑结束----------------------------


// ------------------------mysql标记处理中api逻辑开始----------------------------

// 标记处理中的sql数据化输出
int DBOP_FUN_ExecuteMarkPaymentProcessing(DB_CONNECTION *DBOP_VAR_ExecuteMarkPaymentProcessing_connect, const char *DBOP_VAR_ExecuteMarkPaymentProcessing_ledgerId, const char *DBOP_VAR_ExecuteMarkPaymentProcessing_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkPaymentProcessing is starting", DBOP_VAR_ExecuteMarkPaymentProcessing_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkPaymentProcessing_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkPaymentProcessing_noConstLedgerId, DBOP_VAR_ExecuteMarkPaymentProcessing_ledgerId, sizeof(DBOP_VAR_ExecuteMarkPaymentProcessing_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkPaymentProcessing_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkPaymentProcessing_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkPaymentProcessing_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkPaymentProcessing_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkPaymentProcessing_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkPaymentProcessing_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkPaymentProcessing_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkPaymentProcessing_noConstLedgerId;
    DBOP_VAR_ExecuteMarkPaymentProcessing_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkPaymentProcessing_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkPaymentProcessing_connect->stmt_set_status_payment_processing, DBOP_VAR_ExecuteMarkPaymentProcessing_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark payment processing param: %s", DBOP_VAR_ExecuteMarkPaymentProcessing_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentProcessing_connect->stmt_set_status_payment_processing));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkPaymentProcessing_connect->stmt_set_status_payment_processing)) {
        dzlog_error("[req: %s] Failed to execute mark payment processing statement: %s", DBOP_VAR_ExecuteMarkPaymentProcessing_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentProcessing_connect->stmt_set_status_payment_processing));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkPaymentProcessing_connect->stmt_set_status_payment_processing);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No donation record found with ledger_id: %s or transaction_type is not donation", DBOP_VAR_ExecuteMarkPaymentProcessing_requestId, DBOP_VAR_ExecuteMarkPaymentProcessing_ledgerId);
        return 1; // 表示记录不存在或不是捐赠类型
    }

    dzlog_info("[req: %s] Successfully marked payment as processing for ledger: %s", DBOP_VAR_ExecuteMarkPaymentProcessing_requestId, DBOP_VAR_ExecuteMarkPaymentProcessing_ledgerId);
    return 0;
}

// 标记处理中的API接口
void DBOP_FUN_ApiMarkPaymentProcessing(struct evhttp_request *DBOP_VAR_ApiMarkPaymentProcessing_request, void *DBOP_VAR_ApiMarkPaymentProcessing_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkPaymentProcessing_cfg = (AppConfig *)DBOP_VAR_ApiMarkPaymentProcessing_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkPaymentProcessing_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkPaymentProcessing_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkPaymentProcessing.", DBOP_VAR_ApiMarkPaymentProcessing_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkPaymentProcessing_request, DBOP_VAR_ApiMarkPaymentProcessing_cfg, DBOP_VAR_ApiMarkPaymentProcessing_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkPaymentProcessing_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkPaymentProcessing_request, DBOP_VAR_ApiMarkPaymentProcessing_requestId);
    if (!DBOP_VAR_ApiMarkPaymentProcessing_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkPaymentProcessing_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkPaymentProcessing_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkPaymentProcessing_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkPaymentProcessing_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkPaymentProcessing_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkPaymentProcessing_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkPaymentProcessing_ledgerId = json_string_value(DBOP_VAR_ApiMarkPaymentProcessing_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkPaymentProcessing: ledgerId=%s", DBOP_VAR_ApiMarkPaymentProcessing_requestId, DBOP_VAR_ApiMarkPaymentProcessing_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkPaymentProcessing_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkPaymentProcessing_cfg);
    int result = DBOP_FUN_ExecuteMarkPaymentProcessing(DBOP_VAR_ApiMarkPaymentProcessing_mysqlConnect, DBOP_VAR_ApiMarkPaymentProcessing_ledgerId, DBOP_VAR_ApiMarkPaymentProcessing_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkPaymentProcessing_request, result, DBOP_VAR_ApiMarkPaymentProcessing_requestId, "mark payment processing");

    json_decref(DBOP_VAR_ApiMarkPaymentProcessing_dataJsonAll);
}

// ------------------------mysql标记处理中api逻辑结束----------------------------


// ------------------------mysql标记处理失败api逻辑开始----------------------------

// 标记处理失败的sql数据化输出
int DBOP_FUN_ExecuteMarkPaymentProcessFailed(DB_CONNECTION *DBOP_VAR_ExecuteMarkPaymentProcessFailed_connect, const char *DBOP_VAR_ExecuteMarkPaymentProcessFailed_ledgerId, const char *DBOP_VAR_ExecuteMarkPaymentProcessFailed_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkPaymentProcessFailed is starting", DBOP_VAR_ExecuteMarkPaymentProcessFailed_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkPaymentProcessFailed_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkPaymentProcessFailed_noConstLedgerId, DBOP_VAR_ExecuteMarkPaymentProcessFailed_ledgerId, sizeof(DBOP_VAR_ExecuteMarkPaymentProcessFailed_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkPaymentProcessFailed_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkPaymentProcessFailed_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkPaymentProcessFailed_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkPaymentProcessFailed_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkPaymentProcessFailed_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkPaymentProcessFailed_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkPaymentProcessFailed_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkPaymentProcessFailed_noConstLedgerId;
    DBOP_VAR_ExecuteMarkPaymentProcessFailed_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkPaymentProcessFailed_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkPaymentProcessFailed_connect->stmt_set_status_payment_process_failed, DBOP_VAR_ExecuteMarkPaymentProcessFailed_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark payment process failed param: %s", DBOP_VAR_ExecuteMarkPaymentProcessFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentProcessFailed_connect->stmt_set_status_payment_process_failed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkPaymentProcessFailed_connect->stmt_set_status_payment_process_failed)) {
        dzlog_error("[req: %s] Failed to execute mark payment process failed statement: %s", DBOP_VAR_ExecuteMarkPaymentProcessFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentProcessFailed_connect->stmt_set_status_payment_process_failed));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkPaymentProcessFailed_connect->stmt_set_status_payment_process_failed);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No donation record found with ledger_id: %s or transaction_type is not donation", DBOP_VAR_ExecuteMarkPaymentProcessFailed_requestId, DBOP_VAR_ExecuteMarkPaymentProcessFailed_ledgerId);
        return 1; // 表示记录不存在或不是捐赠类型
    }

    dzlog_info("[req: %s] Successfully marked payment as process failed for ledger: %s", DBOP_VAR_ExecuteMarkPaymentProcessFailed_requestId, DBOP_VAR_ExecuteMarkPaymentProcessFailed_ledgerId);
    return 0;
}

// 标记处理失败的API接口
void DBOP_FUN_ApiMarkPaymentProcessFailed(struct evhttp_request *DBOP_VAR_ApiMarkPaymentProcessFailed_request, void *DBOP_VAR_ApiMarkPaymentProcessFailed_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkPaymentProcessFailed_cfg = (AppConfig *)DBOP_VAR_ApiMarkPaymentProcessFailed_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkPaymentProcessFailed_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkPaymentProcessFailed_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkPaymentProcessFailed.", DBOP_VAR_ApiMarkPaymentProcessFailed_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkPaymentProcessFailed_request, DBOP_VAR_ApiMarkPaymentProcessFailed_cfg, DBOP_VAR_ApiMarkPaymentProcessFailed_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkPaymentProcessFailed_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkPaymentProcessFailed_request, DBOP_VAR_ApiMarkPaymentProcessFailed_requestId);
    if (!DBOP_VAR_ApiMarkPaymentProcessFailed_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkPaymentProcessFailed_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkPaymentProcessFailed_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkPaymentProcessFailed_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkPaymentProcessFailed_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkPaymentProcessFailed_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkPaymentProcessFailed_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkPaymentProcessFailed_ledgerId = json_string_value(DBOP_VAR_ApiMarkPaymentProcessFailed_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkPaymentProcessFailed: ledgerId=%s", DBOP_VAR_ApiMarkPaymentProcessFailed_requestId, DBOP_VAR_ApiMarkPaymentProcessFailed_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkPaymentProcessFailed_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkPaymentProcessFailed_cfg);
    int result = DBOP_FUN_ExecuteMarkPaymentProcessFailed(DBOP_VAR_ApiMarkPaymentProcessFailed_mysqlConnect, DBOP_VAR_ApiMarkPaymentProcessFailed_ledgerId, DBOP_VAR_ApiMarkPaymentProcessFailed_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkPaymentProcessFailed_request, result, DBOP_VAR_ApiMarkPaymentProcessFailed_requestId, "mark payment process failed");

    json_decref(DBOP_VAR_ApiMarkPaymentProcessFailed_dataJsonAll);
}

// ------------------------mysql标记处理失败api逻辑结束----------------------------


// ------------------------mysql标记退款中api逻辑开始----------------------------

// 标记退款中的sql数据化输出
int DBOP_FUN_ExecuteMarkPaymentRefunding(DB_CONNECTION *DBOP_VAR_ExecuteMarkPaymentRefunding_connect, const char *DBOP_VAR_ExecuteMarkPaymentRefunding_ledgerId, const char *DBOP_VAR_ExecuteMarkPaymentRefunding_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkPaymentRefunding is starting", DBOP_VAR_ExecuteMarkPaymentRefunding_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkPaymentRefunding_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkPaymentRefunding_noConstLedgerId, DBOP_VAR_ExecuteMarkPaymentRefunding_ledgerId, sizeof(DBOP_VAR_ExecuteMarkPaymentRefunding_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkPaymentRefunding_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkPaymentRefunding_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkPaymentRefunding_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkPaymentRefunding_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkPaymentRefunding_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkPaymentRefunding_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkPaymentRefunding_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkPaymentRefunding_noConstLedgerId;
    DBOP_VAR_ExecuteMarkPaymentRefunding_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkPaymentRefunding_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkPaymentRefunding_connect->stmt_set_status_payment_refunding, DBOP_VAR_ExecuteMarkPaymentRefunding_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark payment refunding param: %s", DBOP_VAR_ExecuteMarkPaymentRefunding_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentRefunding_connect->stmt_set_status_payment_refunding));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkPaymentRefunding_connect->stmt_set_status_payment_refunding)) {
        dzlog_error("[req: %s] Failed to execute mark payment refunding statement: %s", DBOP_VAR_ExecuteMarkPaymentRefunding_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentRefunding_connect->stmt_set_status_payment_refunding));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkPaymentRefunding_connect->stmt_set_status_payment_refunding);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No donation record found with ledger_id: %s or transaction_type is not donation", DBOP_VAR_ExecuteMarkPaymentRefunding_requestId, DBOP_VAR_ExecuteMarkPaymentRefunding_ledgerId);
        return 1; // 表示记录不存在或不是捐赠类型
    }

    dzlog_info("[req: %s] Successfully marked payment as refunding for ledger: %s", DBOP_VAR_ExecuteMarkPaymentRefunding_requestId, DBOP_VAR_ExecuteMarkPaymentRefunding_ledgerId);
    return 0;
}

// 标记退款中的API接口
void DBOP_FUN_ApiMarkPaymentRefunding(struct evhttp_request *DBOP_VAR_ApiMarkPaymentRefunding_request, void *DBOP_VAR_ApiMarkPaymentRefunding_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkPaymentRefunding_cfg = (AppConfig *)DBOP_VAR_ApiMarkPaymentRefunding_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkPaymentRefunding_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkPaymentRefunding_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkPaymentRefunding.", DBOP_VAR_ApiMarkPaymentRefunding_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkPaymentRefunding_request, DBOP_VAR_ApiMarkPaymentRefunding_cfg, DBOP_VAR_ApiMarkPaymentRefunding_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkPaymentRefunding_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkPaymentRefunding_request, DBOP_VAR_ApiMarkPaymentRefunding_requestId);
    if (!DBOP_VAR_ApiMarkPaymentRefunding_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkPaymentRefunding_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkPaymentRefunding_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkPaymentRefunding_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkPaymentRefunding_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkPaymentRefunding_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkPaymentRefunding_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkPaymentRefunding_ledgerId = json_string_value(DBOP_VAR_ApiMarkPaymentRefunding_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkPaymentRefunding: ledgerId=%s", DBOP_VAR_ApiMarkPaymentRefunding_requestId, DBOP_VAR_ApiMarkPaymentRefunding_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkPaymentRefunding_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkPaymentRefunding_cfg);
    int result = DBOP_FUN_ExecuteMarkPaymentRefunding(DBOP_VAR_ApiMarkPaymentRefunding_mysqlConnect, DBOP_VAR_ApiMarkPaymentRefunding_ledgerId, DBOP_VAR_ApiMarkPaymentRefunding_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkPaymentRefunding_request, result, DBOP_VAR_ApiMarkPaymentRefunding_requestId, "mark payment refunding");

    json_decref(DBOP_VAR_ApiMarkPaymentRefunding_dataJsonAll);
}

// ------------------------mysql标记退款中api逻辑结束----------------------------


// ------------------------mysql标记退款失败api逻辑开始----------------------------

// 标记退款失败的sql数据化输出
int DBOP_FUN_ExecuteMarkPaymentRefundFailed(DB_CONNECTION *DBOP_VAR_ExecuteMarkPaymentRefundFailed_connect, const char *DBOP_VAR_ExecuteMarkPaymentRefundFailed_ledgerId, const char *DBOP_VAR_ExecuteMarkPaymentRefundFailed_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkPaymentRefundFailed is starting", DBOP_VAR_ExecuteMarkPaymentRefundFailed_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkPaymentRefundFailed_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkPaymentRefundFailed_noConstLedgerId, DBOP_VAR_ExecuteMarkPaymentRefundFailed_ledgerId, sizeof(DBOP_VAR_ExecuteMarkPaymentRefundFailed_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkPaymentRefundFailed_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkPaymentRefundFailed_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkPaymentRefundFailed_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkPaymentRefundFailed_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkPaymentRefundFailed_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkPaymentRefundFailed_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkPaymentRefundFailed_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkPaymentRefundFailed_noConstLedgerId;
    DBOP_VAR_ExecuteMarkPaymentRefundFailed_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkPaymentRefundFailed_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkPaymentRefundFailed_connect->stmt_set_status_payment_refund_failed, DBOP_VAR_ExecuteMarkPaymentRefundFailed_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark payment refund failed param: %s", DBOP_VAR_ExecuteMarkPaymentRefundFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentRefundFailed_connect->stmt_set_status_payment_refund_failed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkPaymentRefundFailed_connect->stmt_set_status_payment_refund_failed)) {
        dzlog_error("[req: %s] Failed to execute mark payment refund failed statement: %s", DBOP_VAR_ExecuteMarkPaymentRefundFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentRefundFailed_connect->stmt_set_status_payment_refund_failed));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkPaymentRefundFailed_connect->stmt_set_status_payment_refund_failed);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No donation record found with ledger_id: %s or transaction_type is not donation", DBOP_VAR_ExecuteMarkPaymentRefundFailed_requestId, DBOP_VAR_ExecuteMarkPaymentRefundFailed_ledgerId);
        return 1; // 表示记录不存在或不是捐赠类型
    }

    dzlog_info("[req: %s] Successfully marked payment as refund failed for ledger: %s", DBOP_VAR_ExecuteMarkPaymentRefundFailed_requestId, DBOP_VAR_ExecuteMarkPaymentRefundFailed_ledgerId);
    return 0;
}

// 标记退款失败的API接口
void DBOP_FUN_ApiMarkPaymentRefundFailed(struct evhttp_request *DBOP_VAR_ApiMarkPaymentRefundFailed_request, void *DBOP_VAR_ApiMarkPaymentRefundFailed_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkPaymentRefundFailed_cfg = (AppConfig *)DBOP_VAR_ApiMarkPaymentRefundFailed_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkPaymentRefundFailed_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkPaymentRefundFailed_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkPaymentRefundFailed.", DBOP_VAR_ApiMarkPaymentRefundFailed_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkPaymentRefundFailed_request, DBOP_VAR_ApiMarkPaymentRefundFailed_cfg, DBOP_VAR_ApiMarkPaymentRefundFailed_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkPaymentRefundFailed_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkPaymentRefundFailed_request, DBOP_VAR_ApiMarkPaymentRefundFailed_requestId);
    if (!DBOP_VAR_ApiMarkPaymentRefundFailed_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkPaymentRefundFailed_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkPaymentRefundFailed_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkPaymentRefundFailed_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkPaymentRefundFailed_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkPaymentRefundFailed_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkPaymentRefundFailed_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkPaymentRefundFailed_ledgerId = json_string_value(DBOP_VAR_ApiMarkPaymentRefundFailed_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkPaymentRefundFailed: ledgerId=%s", DBOP_VAR_ApiMarkPaymentRefundFailed_requestId, DBOP_VAR_ApiMarkPaymentRefundFailed_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkPaymentRefundFailed_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkPaymentRefundFailed_cfg);
    int result = DBOP_FUN_ExecuteMarkPaymentRefundFailed(DBOP_VAR_ApiMarkPaymentRefundFailed_mysqlConnect, DBOP_VAR_ApiMarkPaymentRefundFailed_ledgerId, DBOP_VAR_ApiMarkPaymentRefundFailed_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkPaymentRefundFailed_request, result, DBOP_VAR_ApiMarkPaymentRefundFailed_requestId, "mark payment refund failed");

    json_decref(DBOP_VAR_ApiMarkPaymentRefundFailed_dataJsonAll);
}

// ------------------------mysql标记退款失败api逻辑结束----------------------------


// ------------------------mysql标记退款完成api逻辑开始----------------------------

// 标记退款完成的sql数据化输出
int DBOP_FUN_ExecuteMarkPaymentRefundCompleted(DB_CONNECTION *DBOP_VAR_ExecuteMarkPaymentRefundCompleted_connect, const char *DBOP_VAR_ExecuteMarkPaymentRefundCompleted_ledgerId, const char *DBOP_VAR_ExecuteMarkPaymentRefundCompleted_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkPaymentRefundCompleted is starting", DBOP_VAR_ExecuteMarkPaymentRefundCompleted_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkPaymentRefundCompleted_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_noConstLedgerId, DBOP_VAR_ExecuteMarkPaymentRefundCompleted_ledgerId, sizeof(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkPaymentRefundCompleted_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkPaymentRefundCompleted_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkPaymentRefundCompleted_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkPaymentRefundCompleted_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkPaymentRefundCompleted_noConstLedgerId;
    DBOP_VAR_ExecuteMarkPaymentRefundCompleted_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_connect->stmt_set_status_payment_refund_completed, DBOP_VAR_ExecuteMarkPaymentRefundCompleted_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark payment refund completed param: %s", DBOP_VAR_ExecuteMarkPaymentRefundCompleted_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_connect->stmt_set_status_payment_refund_completed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_connect->stmt_set_status_payment_refund_completed)) {
        dzlog_error("[req: %s] Failed to execute mark payment refund completed statement: %s", DBOP_VAR_ExecuteMarkPaymentRefundCompleted_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_connect->stmt_set_status_payment_refund_completed));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkPaymentRefundCompleted_connect->stmt_set_status_payment_refund_completed);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No donation record found with ledger_id: %s or transaction_type is not donation", DBOP_VAR_ExecuteMarkPaymentRefundCompleted_requestId, DBOP_VAR_ExecuteMarkPaymentRefundCompleted_ledgerId);
        return 1; // 表示记录不存在或不是捐赠类型
    }

    dzlog_info("[req: %s] Successfully marked payment as refund completed for ledger: %s", DBOP_VAR_ExecuteMarkPaymentRefundCompleted_requestId, DBOP_VAR_ExecuteMarkPaymentRefundCompleted_ledgerId);
    return 0;
}

// 标记退款完成的API接口
void DBOP_FUN_ApiMarkPaymentRefundCompleted(struct evhttp_request *DBOP_VAR_ApiMarkPaymentRefundCompleted_request, void *DBOP_VAR_ApiMarkPaymentRefundCompleted_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkPaymentRefundCompleted_cfg = (AppConfig *)DBOP_VAR_ApiMarkPaymentRefundCompleted_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkPaymentRefundCompleted_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkPaymentRefundCompleted_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkPaymentRefundCompleted.", DBOP_VAR_ApiMarkPaymentRefundCompleted_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkPaymentRefundCompleted_request, DBOP_VAR_ApiMarkPaymentRefundCompleted_cfg, DBOP_VAR_ApiMarkPaymentRefundCompleted_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkPaymentRefundCompleted_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkPaymentRefundCompleted_request, DBOP_VAR_ApiMarkPaymentRefundCompleted_requestId);
    if (!DBOP_VAR_ApiMarkPaymentRefundCompleted_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkPaymentRefundCompleted_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkPaymentRefundCompleted_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkPaymentRefundCompleted_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkPaymentRefundCompleted_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkPaymentRefundCompleted_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkPaymentRefundCompleted_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkPaymentRefundCompleted_ledgerId = json_string_value(DBOP_VAR_ApiMarkPaymentRefundCompleted_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkPaymentRefundCompleted: ledgerId=%s", DBOP_VAR_ApiMarkPaymentRefundCompleted_requestId, DBOP_VAR_ApiMarkPaymentRefundCompleted_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkPaymentRefundCompleted_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkPaymentRefundCompleted_cfg);
    int result = DBOP_FUN_ExecuteMarkPaymentRefundCompleted(DBOP_VAR_ApiMarkPaymentRefundCompleted_mysqlConnect, DBOP_VAR_ApiMarkPaymentRefundCompleted_ledgerId, DBOP_VAR_ApiMarkPaymentRefundCompleted_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkPaymentRefundCompleted_request, result, DBOP_VAR_ApiMarkPaymentRefundCompleted_requestId, "mark payment refund completed");

    json_decref(DBOP_VAR_ApiMarkPaymentRefundCompleted_dataJsonAll);
}

// ------------------------mysql标记退款完成api逻辑结束----------------------------


// ------------------------mysql标记完成api逻辑开始----------------------------

// 标记完成的sql数据化输出
int DBOP_FUN_ExecuteMarkPaymentCompleted(DB_CONNECTION *DBOP_VAR_ExecuteMarkPaymentCompleted_connect, const char *DBOP_VAR_ExecuteMarkPaymentCompleted_ledgerId, const char *DBOP_VAR_ExecuteMarkPaymentCompleted_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkPaymentCompleted is starting", DBOP_VAR_ExecuteMarkPaymentCompleted_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkPaymentCompleted_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkPaymentCompleted_noConstLedgerId, DBOP_VAR_ExecuteMarkPaymentCompleted_ledgerId, sizeof(DBOP_VAR_ExecuteMarkPaymentCompleted_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkPaymentCompleted_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkPaymentCompleted_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkPaymentCompleted_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkPaymentCompleted_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkPaymentCompleted_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkPaymentCompleted_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkPaymentCompleted_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkPaymentCompleted_noConstLedgerId;
    DBOP_VAR_ExecuteMarkPaymentCompleted_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkPaymentCompleted_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkPaymentCompleted_connect->stmt_set_status_payment_completed, DBOP_VAR_ExecuteMarkPaymentCompleted_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark payment completed param: %s", DBOP_VAR_ExecuteMarkPaymentCompleted_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentCompleted_connect->stmt_set_status_payment_completed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkPaymentCompleted_connect->stmt_set_status_payment_completed)) {
        dzlog_error("[req: %s] Failed to execute mark payment completed statement: %s", DBOP_VAR_ExecuteMarkPaymentCompleted_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkPaymentCompleted_connect->stmt_set_status_payment_completed));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkPaymentCompleted_connect->stmt_set_status_payment_completed);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No donation record found with ledger_id: %s or transaction_type is not donation", DBOP_VAR_ExecuteMarkPaymentCompleted_requestId, DBOP_VAR_ExecuteMarkPaymentCompleted_ledgerId);
        return 1; // 表示记录不存在或不是捐赠类型
    }

    dzlog_info("[req: %s] Successfully marked payment as completed for ledger: %s", DBOP_VAR_ExecuteMarkPaymentCompleted_requestId, DBOP_VAR_ExecuteMarkPaymentCompleted_ledgerId);
    return 0;
}

// 标记完成的API接口
void DBOP_FUN_ApiMarkPaymentCompleted(struct evhttp_request *DBOP_VAR_ApiMarkPaymentCompleted_request, void *DBOP_VAR_ApiMarkPaymentCompleted_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkPaymentCompleted_cfg = (AppConfig *)DBOP_VAR_ApiMarkPaymentCompleted_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkPaymentCompleted_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkPaymentCompleted_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkPaymentCompleted.", DBOP_VAR_ApiMarkPaymentCompleted_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkPaymentCompleted_request, DBOP_VAR_ApiMarkPaymentCompleted_cfg, DBOP_VAR_ApiMarkPaymentCompleted_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkPaymentCompleted_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkPaymentCompleted_request, DBOP_VAR_ApiMarkPaymentCompleted_requestId);
    if (!DBOP_VAR_ApiMarkPaymentCompleted_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkPaymentCompleted_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkPaymentCompleted_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkPaymentCompleted_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkPaymentCompleted_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkPaymentCompleted_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkPaymentCompleted_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkPaymentCompleted_ledgerId = json_string_value(DBOP_VAR_ApiMarkPaymentCompleted_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkPaymentCompleted: ledgerId=%s", DBOP_VAR_ApiMarkPaymentCompleted_requestId, DBOP_VAR_ApiMarkPaymentCompleted_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkPaymentCompleted_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkPaymentCompleted_cfg);
    int result = DBOP_FUN_ExecuteMarkPaymentCompleted(DBOP_VAR_ApiMarkPaymentCompleted_mysqlConnect, DBOP_VAR_ApiMarkPaymentCompleted_ledgerId, DBOP_VAR_ApiMarkPaymentCompleted_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkPaymentCompleted_request, result, DBOP_VAR_ApiMarkPaymentCompleted_requestId, "mark payment completed");

    json_decref(DBOP_VAR_ApiMarkPaymentCompleted_dataJsonAll);
}

// ------------------------mysql标记完成api逻辑结束----------------------------


// ------------------------mysql新增退款待选择记录api逻辑开始----------------------------

// 新增退款待选择记录的sql数据化输出
int DBOP_FUN_ExecuteCreateRefundPendingSelection(DB_CONNECTION *DBOP_VAR_ExecuteCreateRefundPendingSelection_connect, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_ledgerId, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_projectId, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_userId, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_suffererUserId, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_donorUserName, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_suffererRealName, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_suffererUserName, int DBOP_VAR_ExecuteCreateRefundPendingSelection_amount, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_note, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_targetUserId, const char *DBOP_VAR_ExecuteCreateRefundPendingSelection_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteCreateRefundPendingSelection is starting", DBOP_VAR_ExecuteCreateRefundPendingSelection_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstLedgerId[256];
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstProjectId[256];
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstUserId[256];
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserId[256];
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstDonorUserName[256];
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererRealName[256];
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserName[256];
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstNote[1024];
    char DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstTargetUserId[256];

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstLedgerId, DBOP_VAR_ExecuteCreateRefundPendingSelection_ledgerId, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstLedgerId[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstLedgerId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstProjectId, DBOP_VAR_ExecuteCreateRefundPendingSelection_projectId, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstProjectId) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstProjectId[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstProjectId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstUserId, DBOP_VAR_ExecuteCreateRefundPendingSelection_userId, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstUserId) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstUserId[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserId, DBOP_VAR_ExecuteCreateRefundPendingSelection_suffererUserId, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserId) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserId[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserId) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstDonorUserName, DBOP_VAR_ExecuteCreateRefundPendingSelection_donorUserName, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstDonorUserName) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstDonorUserName[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstDonorUserName) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererRealName, DBOP_VAR_ExecuteCreateRefundPendingSelection_suffererRealName, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererRealName) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererRealName[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererRealName) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserName, DBOP_VAR_ExecuteCreateRefundPendingSelection_suffererUserName, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserName) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserName[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserName) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstNote, DBOP_VAR_ExecuteCreateRefundPendingSelection_note, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstNote) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstNote[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstNote) - 1] = '\0';

    strncpy(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstTargetUserId, DBOP_VAR_ExecuteCreateRefundPendingSelection_targetUserId, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstTargetUserId) - 1);
    DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstTargetUserId[sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstTargetUserId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[10];
    memset(DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams, 0, sizeof(DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams));

    // 绑定参数
    // ledger_id
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstLedgerId;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstLedgerId);

    // project_id
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstProjectId;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstProjectId);

    // user_id
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[2].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstUserId;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[2].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstUserId);

    // sufferer_user_id
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[3].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[3].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserId;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[3].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserId);

    // donor_user_name
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[4].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[4].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstDonorUserName;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[4].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstDonorUserName);

    // sufferer_real_name
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[5].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[5].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererRealName;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[5].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererRealName);

    // sufferer_user_name
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[6].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[6].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserName;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[6].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstSuffererUserName);

    // amount
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[7].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[7].buffer = (char *)&DBOP_VAR_ExecuteCreateRefundPendingSelection_amount;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[7].is_null = 0;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[7].length = 0;

    // note
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[8].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[8].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstNote;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[8].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstNote);

    // target_user_id
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[9].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[9].buffer = (char *)DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstTargetUserId;
    DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams[9].buffer_length = strlen(DBOP_VAR_ExecuteCreateRefundPendingSelection_noConstTargetUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreateRefundPendingSelection_connect->stmt_create_refund_pending_selection, DBOP_VAR_ExecuteCreateRefundPendingSelection_bindParams)) {
        dzlog_error("[req: %s] Failed to bind create refund pending selection param: %s", DBOP_VAR_ExecuteCreateRefundPendingSelection_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateRefundPendingSelection_connect->stmt_create_refund_pending_selection));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreateRefundPendingSelection_connect->stmt_create_refund_pending_selection)) {
        dzlog_error("[req: %s] Failed to execute create refund pending selection statement: %s", DBOP_VAR_ExecuteCreateRefundPendingSelection_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateRefundPendingSelection_connect->stmt_create_refund_pending_selection));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteCreateRefundPendingSelection_connect->stmt_create_refund_pending_selection);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] Failed to create refund pending selection record", DBOP_VAR_ExecuteCreateRefundPendingSelection_requestId);
        return 1;
    }

    dzlog_info("[req: %s] Successfully created refund pending selection record with ledger_id: %s", DBOP_VAR_ExecuteCreateRefundPendingSelection_requestId, DBOP_VAR_ExecuteCreateRefundPendingSelection_ledgerId);
    return 0;
}

// 新增退款待选择记录的API接口
void DBOP_FUN_ApiCreateRefundPendingSelection(struct evhttp_request *DBOP_VAR_ApiCreateRefundPendingSelection_request, void *DBOP_VAR_ApiCreateRefundPendingSelection_voidCfg) {
    AppConfig *DBOP_VAR_ApiCreateRefundPendingSelection_cfg = (AppConfig *)DBOP_VAR_ApiCreateRefundPendingSelection_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiCreateRefundPendingSelection_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiCreateRefundPendingSelection.", DBOP_VAR_ApiCreateRefundPendingSelection_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiCreateRefundPendingSelection_request, DBOP_VAR_ApiCreateRefundPendingSelection_cfg, DBOP_VAR_ApiCreateRefundPendingSelection_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiCreateRefundPendingSelection_request, DBOP_VAR_ApiCreateRefundPendingSelection_requestId);
    if (!DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "ledger_id");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonProjectId = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonUserId = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererUserId = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "sufferer_user_id");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonDonorUserName = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "donor_user_name");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererRealName = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "sufferer_real_name");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererUserName = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "sufferer_user_name");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAmount = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "amount");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonNote = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "note");
    json_t *DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonTargetUserId = json_object_get(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll, "target_user_id");

    if (!json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonLedgerId) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonProjectId) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererUserId) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonDonorUserName) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererRealName) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererUserName) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAmount) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonNote) ||
        !json_is_string(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonTargetUserId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string types for all fields", DBOP_VAR_ApiCreateRefundPendingSelection_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreateRefundPendingSelection_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiCreateRefundPendingSelection_ledgerId = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonLedgerId);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_projectId = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonProjectId);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_userId = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonUserId);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_suffererUserId = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererUserId);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_donorUserName = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonDonorUserName);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_suffererRealName = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererRealName);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_suffererUserName = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonSuffererUserName);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_amountStr = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAmount);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_note = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonNote);
    const char *DBOP_VAR_ApiCreateRefundPendingSelection_targetUserId = json_string_value(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonTargetUserId);

    // 转换金额字符串为整数
    int DBOP_VAR_ApiCreateRefundPendingSelection_amount = atoi(DBOP_VAR_ApiCreateRefundPendingSelection_amountStr);

    dzlog_info("[req: %s] Executing database operation for ApiCreateRefundPendingSelection: ledgerId=%s, projectId=%s, amount=%d", 
               DBOP_VAR_ApiCreateRefundPendingSelection_requestId, 
               DBOP_VAR_ApiCreateRefundPendingSelection_ledgerId, 
               DBOP_VAR_ApiCreateRefundPendingSelection_projectId, 
               DBOP_VAR_ApiCreateRefundPendingSelection_amount);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiCreateRefundPendingSelection_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiCreateRefundPendingSelection_cfg);
    int result = DBOP_FUN_ExecuteCreateRefundPendingSelection(DBOP_VAR_ApiCreateRefundPendingSelection_mysqlConnect, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_ledgerId, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_projectId, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_userId, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_suffererUserId, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_donorUserName, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_suffererRealName, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_suffererUserName, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_amount, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_note, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_targetUserId, 
                                                             DBOP_VAR_ApiCreateRefundPendingSelection_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiCreateRefundPendingSelection_request, result, DBOP_VAR_ApiCreateRefundPendingSelection_requestId, "create refund pending selection");

    json_decref(DBOP_VAR_ApiCreateRefundPendingSelection_dataJsonAll);
}

// ------------------------mysql新增退款待选择记录api逻辑结束----------------------------


// ------------------------mysql标记退款待选择转为退款类型api逻辑开始----------------------------

// 标记退款待选择转为退款类型的sql数据化输出
int DBOP_FUN_ExecuteMarkRefundSelected(DB_CONNECTION *DBOP_VAR_ExecuteMarkRefundSelected_connect, const char *DBOP_VAR_ExecuteMarkRefundSelected_ledgerId, const char *DBOP_VAR_ExecuteMarkRefundSelected_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkRefundSelected is starting", DBOP_VAR_ExecuteMarkRefundSelected_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkRefundSelected_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkRefundSelected_noConstLedgerId, DBOP_VAR_ExecuteMarkRefundSelected_ledgerId, sizeof(DBOP_VAR_ExecuteMarkRefundSelected_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkRefundSelected_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkRefundSelected_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkRefundSelected_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkRefundSelected_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkRefundSelected_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkRefundSelected_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkRefundSelected_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkRefundSelected_noConstLedgerId;
    DBOP_VAR_ExecuteMarkRefundSelected_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkRefundSelected_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkRefundSelected_connect->stmt_set_type_refund, DBOP_VAR_ExecuteMarkRefundSelected_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark refund selected param: %s", DBOP_VAR_ExecuteMarkRefundSelected_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkRefundSelected_connect->stmt_set_type_refund));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkRefundSelected_connect->stmt_set_type_refund)) {
        dzlog_error("[req: %s] Failed to execute mark refund selected statement: %s", DBOP_VAR_ExecuteMarkRefundSelected_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkRefundSelected_connect->stmt_set_type_refund));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkRefundSelected_connect->stmt_set_type_refund);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No refund pending selection record found with ledger_id: %s or transaction_type is not refund pending selection", DBOP_VAR_ExecuteMarkRefundSelected_requestId, DBOP_VAR_ExecuteMarkRefundSelected_ledgerId);
        return 1; // 表示记录不存在或不是退款待选择类型
    }

    dzlog_info("[req: %s] Successfully marked refund pending selection as refund type for ledger: %s", DBOP_VAR_ExecuteMarkRefundSelected_requestId, DBOP_VAR_ExecuteMarkRefundSelected_ledgerId);
    return 0;
}

// 标记退款待选择转为退款类型的API接口
void DBOP_FUN_ApiMarkRefundSelected(struct evhttp_request *DBOP_VAR_ApiMarkRefundSelected_request, void *DBOP_VAR_ApiMarkRefundSelected_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkRefundSelected_cfg = (AppConfig *)DBOP_VAR_ApiMarkRefundSelected_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkRefundSelected_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkRefundSelected_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkRefundSelected.", DBOP_VAR_ApiMarkRefundSelected_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkRefundSelected_request, DBOP_VAR_ApiMarkRefundSelected_cfg, DBOP_VAR_ApiMarkRefundSelected_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkRefundSelected_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkRefundSelected_request, DBOP_VAR_ApiMarkRefundSelected_requestId);
    if (!DBOP_VAR_ApiMarkRefundSelected_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkRefundSelected_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkRefundSelected_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkRefundSelected_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkRefundSelected_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkRefundSelected_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkRefundSelected_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkRefundSelected_ledgerId = json_string_value(DBOP_VAR_ApiMarkRefundSelected_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkRefundSelected: ledgerId=%s", DBOP_VAR_ApiMarkRefundSelected_requestId, DBOP_VAR_ApiMarkRefundSelected_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkRefundSelected_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkRefundSelected_cfg);
    int result = DBOP_FUN_ExecuteMarkRefundSelected(DBOP_VAR_ApiMarkRefundSelected_mysqlConnect, DBOP_VAR_ApiMarkRefundSelected_ledgerId, DBOP_VAR_ApiMarkRefundSelected_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkRefundSelected_request, result, DBOP_VAR_ApiMarkRefundSelected_requestId, "mark refund selected");

    json_decref(DBOP_VAR_ApiMarkRefundSelected_dataJsonAll);
}

// ------------------------mysql标记退款待选择转为退款类型api逻辑结束----------------------------


// ------------------------mysql标记退款失败api逻辑开始----------------------------

// 标记退款失败的sql数据化输出
int DBOP_FUN_ExecuteMarkRefundFailed(DB_CONNECTION *DBOP_VAR_ExecuteMarkRefundFailed_connect, const char *DBOP_VAR_ExecuteMarkRefundFailed_ledgerId, const char *DBOP_VAR_ExecuteMarkRefundFailed_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkRefundFailed is starting", DBOP_VAR_ExecuteMarkRefundFailed_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkRefundFailed_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkRefundFailed_noConstLedgerId, DBOP_VAR_ExecuteMarkRefundFailed_ledgerId, sizeof(DBOP_VAR_ExecuteMarkRefundFailed_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkRefundFailed_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkRefundFailed_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkRefundFailed_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkRefundFailed_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkRefundFailed_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkRefundFailed_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkRefundFailed_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkRefundFailed_noConstLedgerId;
    DBOP_VAR_ExecuteMarkRefundFailed_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkRefundFailed_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkRefundFailed_connect->stmt_set_status_refund_failed, DBOP_VAR_ExecuteMarkRefundFailed_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark refund failed param: %s", DBOP_VAR_ExecuteMarkRefundFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkRefundFailed_connect->stmt_set_status_refund_failed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkRefundFailed_connect->stmt_set_status_refund_failed)) {
        dzlog_error("[req: %s] Failed to execute mark refund failed statement: %s", DBOP_VAR_ExecuteMarkRefundFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkRefundFailed_connect->stmt_set_status_refund_failed));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkRefundFailed_connect->stmt_set_status_refund_failed);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No refund record found with ledger_id: %s or transaction_type is not refund", DBOP_VAR_ExecuteMarkRefundFailed_requestId, DBOP_VAR_ExecuteMarkRefundFailed_ledgerId);
        return 1; // 表示记录不存在或不是退款类型
    }

    dzlog_info("[req: %s] Successfully marked refund as failed for ledger: %s", DBOP_VAR_ExecuteMarkRefundFailed_requestId, DBOP_VAR_ExecuteMarkRefundFailed_ledgerId);
    return 0;
}

// 标记退款失败的API接口
void DBOP_FUN_ApiMarkRefundFailed(struct evhttp_request *DBOP_VAR_ApiMarkRefundFailed_request, void *DBOP_VAR_ApiMarkRefundFailed_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkRefundFailed_cfg = (AppConfig *)DBOP_VAR_ApiMarkRefundFailed_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkRefundFailed_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkRefundFailed_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkRefundFailed.", DBOP_VAR_ApiMarkRefundFailed_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkRefundFailed_request, DBOP_VAR_ApiMarkRefundFailed_cfg, DBOP_VAR_ApiMarkRefundFailed_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkRefundFailed_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkRefundFailed_request, DBOP_VAR_ApiMarkRefundFailed_requestId);
    if (!DBOP_VAR_ApiMarkRefundFailed_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkRefundFailed_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkRefundFailed_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkRefundFailed_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkRefundFailed_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkRefundFailed_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkRefundFailed_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkRefundFailed_ledgerId = json_string_value(DBOP_VAR_ApiMarkRefundFailed_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkRefundFailed: ledgerId=%s", DBOP_VAR_ApiMarkRefundFailed_requestId, DBOP_VAR_ApiMarkRefundFailed_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkRefundFailed_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkRefundFailed_cfg);
    int result = DBOP_FUN_ExecuteMarkRefundFailed(DBOP_VAR_ApiMarkRefundFailed_mysqlConnect, DBOP_VAR_ApiMarkRefundFailed_ledgerId, DBOP_VAR_ApiMarkRefundFailed_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkRefundFailed_request, result, DBOP_VAR_ApiMarkRefundFailed_requestId, "mark refund failed");

    json_decref(DBOP_VAR_ApiMarkRefundFailed_dataJsonAll);
}

// ------------------------mysql标记退款失败api逻辑结束----------------------------


// ------------------------mysql标记退款完成api逻辑开始----------------------------

// 标记退款完成的sql数据化输出
int DBOP_FUN_ExecuteMarkRefundCompleted(DB_CONNECTION *DBOP_VAR_ExecuteMarkRefundCompleted_connect, const char *DBOP_VAR_ExecuteMarkRefundCompleted_ledgerId, const char *DBOP_VAR_ExecuteMarkRefundCompleted_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkRefundCompleted is starting", DBOP_VAR_ExecuteMarkRefundCompleted_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkRefundCompleted_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkRefundCompleted_noConstLedgerId, DBOP_VAR_ExecuteMarkRefundCompleted_ledgerId, sizeof(DBOP_VAR_ExecuteMarkRefundCompleted_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkRefundCompleted_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkRefundCompleted_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkRefundCompleted_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkRefundCompleted_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkRefundCompleted_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkRefundCompleted_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkRefundCompleted_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkRefundCompleted_noConstLedgerId;
    DBOP_VAR_ExecuteMarkRefundCompleted_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkRefundCompleted_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkRefundCompleted_connect->stmt_set_status_refund_completed, DBOP_VAR_ExecuteMarkRefundCompleted_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark refund completed param: %s", DBOP_VAR_ExecuteMarkRefundCompleted_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkRefundCompleted_connect->stmt_set_status_refund_completed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkRefundCompleted_connect->stmt_set_status_refund_completed)) {
        dzlog_error("[req: %s] Failed to execute mark refund completed statement: %s", DBOP_VAR_ExecuteMarkRefundCompleted_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkRefundCompleted_connect->stmt_set_status_refund_completed));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkRefundCompleted_connect->stmt_set_status_refund_completed);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No refund record found with ledger_id: %s or transaction_type is not refund", DBOP_VAR_ExecuteMarkRefundCompleted_requestId, DBOP_VAR_ExecuteMarkRefundCompleted_ledgerId);
        return 1; // 表示记录不存在或不是退款类型
    }

    dzlog_info("[req: %s] Successfully marked refund as completed for ledger: %s", DBOP_VAR_ExecuteMarkRefundCompleted_requestId, DBOP_VAR_ExecuteMarkRefundCompleted_ledgerId);
    return 0;
}

// 标记退款完成的API接口
void DBOP_FUN_ApiMarkRefundCompleted(struct evhttp_request *DBOP_VAR_ApiMarkRefundCompleted_request, void *DBOP_VAR_ApiMarkRefundCompleted_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkRefundCompleted_cfg = (AppConfig *)DBOP_VAR_ApiMarkRefundCompleted_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkRefundCompleted_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkRefundCompleted_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkRefundCompleted.", DBOP_VAR_ApiMarkRefundCompleted_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkRefundCompleted_request, DBOP_VAR_ApiMarkRefundCompleted_cfg, DBOP_VAR_ApiMarkRefundCompleted_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkRefundCompleted_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkRefundCompleted_request, DBOP_VAR_ApiMarkRefundCompleted_requestId);
    if (!DBOP_VAR_ApiMarkRefundCompleted_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkRefundCompleted_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkRefundCompleted_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkRefundCompleted_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkRefundCompleted_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkRefundCompleted_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkRefundCompleted_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkRefundCompleted_ledgerId = json_string_value(DBOP_VAR_ApiMarkRefundCompleted_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkRefundCompleted: ledgerId=%s", DBOP_VAR_ApiMarkRefundCompleted_requestId, DBOP_VAR_ApiMarkRefundCompleted_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkRefundCompleted_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkRefundCompleted_cfg);
    int result = DBOP_FUN_ExecuteMarkRefundCompleted(DBOP_VAR_ApiMarkRefundCompleted_mysqlConnect, DBOP_VAR_ApiMarkRefundCompleted_ledgerId, DBOP_VAR_ApiMarkRefundCompleted_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkRefundCompleted_request, result, DBOP_VAR_ApiMarkRefundCompleted_requestId, "mark refund completed");

    json_decref(DBOP_VAR_ApiMarkRefundCompleted_dataJsonAll);
}

// ------------------------mysql标记退款完成api逻辑结束----------------------------


// ------------------------mysql标记退款待选择转为转应急池类型api逻辑开始----------------------------

// 标记退款待选择转为转应急池类型的sql数据化输出
int DBOP_FUN_ExecuteMarkEmergencyPool(DB_CONNECTION *DBOP_VAR_ExecuteMarkEmergencyPool_connect, const char *DBOP_VAR_ExecuteMarkEmergencyPool_ledgerId, const char *DBOP_VAR_ExecuteMarkEmergencyPool_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteMarkEmergencyPool is starting", DBOP_VAR_ExecuteMarkEmergencyPool_requestId);

    // 保护原始参数不被修改，复制参数
    char DBOP_VAR_ExecuteMarkEmergencyPool_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteMarkEmergencyPool_noConstLedgerId, DBOP_VAR_ExecuteMarkEmergencyPool_ledgerId, sizeof(DBOP_VAR_ExecuteMarkEmergencyPool_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteMarkEmergencyPool_noConstLedgerId[sizeof(DBOP_VAR_ExecuteMarkEmergencyPool_noConstLedgerId) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteMarkEmergencyPool_bindParams[1];
    memset(DBOP_VAR_ExecuteMarkEmergencyPool_bindParams, 0, sizeof(DBOP_VAR_ExecuteMarkEmergencyPool_bindParams));

    // 绑定参数：ledger_id
    DBOP_VAR_ExecuteMarkEmergencyPool_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteMarkEmergencyPool_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteMarkEmergencyPool_noConstLedgerId;
    DBOP_VAR_ExecuteMarkEmergencyPool_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteMarkEmergencyPool_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteMarkEmergencyPool_connect->stmt_set_type_emergency_pool, DBOP_VAR_ExecuteMarkEmergencyPool_bindParams)) {
        dzlog_error("[req: %s] Failed to bind mark emergency pool param: %s", DBOP_VAR_ExecuteMarkEmergencyPool_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkEmergencyPool_connect->stmt_set_type_emergency_pool));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteMarkEmergencyPool_connect->stmt_set_type_emergency_pool)) {
        dzlog_error("[req: %s] Failed to execute mark emergency pool statement: %s", DBOP_VAR_ExecuteMarkEmergencyPool_requestId, mysql_stmt_error(DBOP_VAR_ExecuteMarkEmergencyPool_connect->stmt_set_type_emergency_pool));
        return -1;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteMarkEmergencyPool_connect->stmt_set_type_emergency_pool);
    if (affected_rows == 0) {
        dzlog_warn("[req: %s] No refund pending selection record found with ledger_id: %s or transaction_type is not refund pending selection", DBOP_VAR_ExecuteMarkEmergencyPool_requestId, DBOP_VAR_ExecuteMarkEmergencyPool_ledgerId);
        return 1; // 表示记录不存在或不是退款待选择类型
    }

    dzlog_info("[req: %s] Successfully marked refund pending selection as emergency pool for ledger: %s", DBOP_VAR_ExecuteMarkEmergencyPool_requestId, DBOP_VAR_ExecuteMarkEmergencyPool_ledgerId);
    return 0;
}

// 标记退款待选择转为转应急池类型的API接口
void DBOP_FUN_ApiMarkEmergencyPool(struct evhttp_request *DBOP_VAR_ApiMarkEmergencyPool_request, void *DBOP_VAR_ApiMarkEmergencyPool_voidCfg) {
    AppConfig *DBOP_VAR_ApiMarkEmergencyPool_cfg = (AppConfig *)DBOP_VAR_ApiMarkEmergencyPool_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiMarkEmergencyPool_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiMarkEmergencyPool_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiMarkEmergencyPool.", DBOP_VAR_ApiMarkEmergencyPool_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiMarkEmergencyPool_request, DBOP_VAR_ApiMarkEmergencyPool_cfg, DBOP_VAR_ApiMarkEmergencyPool_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiMarkEmergencyPool_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiMarkEmergencyPool_request, DBOP_VAR_ApiMarkEmergencyPool_requestId);
    if (!DBOP_VAR_ApiMarkEmergencyPool_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiMarkEmergencyPool_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiMarkEmergencyPool_dataJsonAll, "ledger_id");

    if (!json_is_string(DBOP_VAR_ApiMarkEmergencyPool_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string type for 'ledger_id'", DBOP_VAR_ApiMarkEmergencyPool_requestId);
        evhttp_send_reply(DBOP_VAR_ApiMarkEmergencyPool_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiMarkEmergencyPool_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiMarkEmergencyPool_ledgerId = json_string_value(DBOP_VAR_ApiMarkEmergencyPool_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation for ApiMarkEmergencyPool: ledgerId=%s", DBOP_VAR_ApiMarkEmergencyPool_requestId, DBOP_VAR_ApiMarkEmergencyPool_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiMarkEmergencyPool_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiMarkEmergencyPool_cfg);
    int result = DBOP_FUN_ExecuteMarkEmergencyPool(DBOP_VAR_ApiMarkEmergencyPool_mysqlConnect, DBOP_VAR_ApiMarkEmergencyPool_ledgerId, DBOP_VAR_ApiMarkEmergencyPool_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiMarkEmergencyPool_request, result, DBOP_VAR_ApiMarkEmergencyPool_requestId, "mark emergency pool");

    json_decref(DBOP_VAR_ApiMarkEmergencyPool_dataJsonAll);
}

// ------------------------mysql标记退款待选择转为转应急池类型api逻辑结束----------------------------


// ------------------------mysql新增患者使用记录api逻辑开始----------------------------

// 新增患者使用记录的sql数据化输出
int DBOP_FUN_ExecuteCreatePatientUsage(DB_CONNECTION *DBOP_VAR_ExecuteCreatePatientUsage_connect, const char *DBOP_VAR_ExecuteCreatePatientUsage_ledgerId, const char *DBOP_VAR_ExecuteCreatePatientUsage_projectId, const char *DBOP_VAR_ExecuteCreatePatientUsage_userId, const char *DBOP_VAR_ExecuteCreatePatientUsage_suffererUserId, const char *DBOP_VAR_ExecuteCreatePatientUsage_suffererRealName, const char *DBOP_VAR_ExecuteCreatePatientUsage_suffererUserName, int DBOP_VAR_ExecuteCreatePatientUsage_amount, const char *DBOP_VAR_ExecuteCreatePatientUsage_note, int DBOP_VAR_ExecuteCreatePatientUsage_paymentMethod, const char *DBOP_VAR_ExecuteCreatePatientUsage_verificationRecord, const char *DBOP_VAR_ExecuteCreatePatientUsage_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteCreatePatientUsage is starting", DBOP_VAR_ExecuteCreatePatientUsage_requestId);

    // 首先创建志愿者审计记录，使用相同的ID
    char DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId[256];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId, DBOP_VAR_ExecuteCreatePatientUsage_ledgerId, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId) - 1] = '\0';

    char DBOP_VAR_ExecuteCreatePatientUsage_noConstVerificationRecord[4096];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstVerificationRecord, DBOP_VAR_ExecuteCreatePatientUsage_verificationRecord, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstVerificationRecord) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstVerificationRecord[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstVerificationRecord) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams[2];
    memset(DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams, 0, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams));

    // 绑定审计记录参数：audit_id, verification_record
    DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams[0].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId;
    DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId);

    DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams[1].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstVerificationRecord;
    DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstVerificationRecord);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreatePatientUsage_connect->stmt_create_volunteer_audit, DBOP_VAR_ExecuteCreatePatientUsage_auditBindParams)) {
        dzlog_error("[req: %s] Failed to bind volunteer audit param: %s", DBOP_VAR_ExecuteCreatePatientUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreatePatientUsage_connect->stmt_create_volunteer_audit));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreatePatientUsage_connect->stmt_create_volunteer_audit)) {
        dzlog_error("[req: %s] Failed to execute volunteer audit statement: %s", DBOP_VAR_ExecuteCreatePatientUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreatePatientUsage_connect->stmt_create_volunteer_audit));
        return -1;
    }

    dzlog_info("[req: %s] Successfully created volunteer audit record with ID: %s", DBOP_VAR_ExecuteCreatePatientUsage_requestId, DBOP_VAR_ExecuteCreatePatientUsage_ledgerId);

    // 然后创建患者使用记录
    char DBOP_VAR_ExecuteCreatePatientUsage_noConstLedgerId[256];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstLedgerId, DBOP_VAR_ExecuteCreatePatientUsage_ledgerId, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstLedgerId) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstLedgerId[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstLedgerId) - 1] = '\0';

    char DBOP_VAR_ExecuteCreatePatientUsage_noConstProjectId[256];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstProjectId, DBOP_VAR_ExecuteCreatePatientUsage_projectId, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstProjectId) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstProjectId[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstProjectId) - 1] = '\0';

    char DBOP_VAR_ExecuteCreatePatientUsage_noConstUserId[256];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstUserId, DBOP_VAR_ExecuteCreatePatientUsage_userId, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstUserId) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstUserId[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstUserId) - 1] = '\0';

    char DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserId[256];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserId, DBOP_VAR_ExecuteCreatePatientUsage_suffererUserId, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserId) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserId[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserId) - 1] = '\0';

    char DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererRealName[256];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererRealName, DBOP_VAR_ExecuteCreatePatientUsage_suffererRealName, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererRealName) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererRealName[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererRealName) - 1] = '\0';

    char DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserName[256];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserName, DBOP_VAR_ExecuteCreatePatientUsage_suffererUserName, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserName) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserName[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserName) - 1] = '\0';

    char DBOP_VAR_ExecuteCreatePatientUsage_noConstNote[512];
    strncpy(DBOP_VAR_ExecuteCreatePatientUsage_noConstNote, DBOP_VAR_ExecuteCreatePatientUsage_note, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstNote) - 1);
    DBOP_VAR_ExecuteCreatePatientUsage_noConstNote[sizeof(DBOP_VAR_ExecuteCreatePatientUsage_noConstNote) - 1] = '\0';

    MYSQL_BIND DBOP_VAR_ExecuteCreatePatientUsage_bindParams[10];
    memset(DBOP_VAR_ExecuteCreatePatientUsage_bindParams, 0, sizeof(DBOP_VAR_ExecuteCreatePatientUsage_bindParams));

    // 绑定参数：ledger_id, project_id, user_id, sufferer_user_id, sufferer_real_name, sufferer_user_name, amount, note, payment_method, volunteer_audit_id
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstLedgerId;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstLedgerId);

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstProjectId;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstProjectId);

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[2].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstUserId;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[2].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstUserId);

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[3].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[3].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserId;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[3].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserId);

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[4].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[4].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererRealName;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[4].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererRealName);

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[5].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[5].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserName;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[5].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstSuffererUserName);

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[6].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[6].buffer = &DBOP_VAR_ExecuteCreatePatientUsage_amount;

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[7].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[7].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstNote;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[7].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstNote);

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[8].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[8].buffer = &DBOP_VAR_ExecuteCreatePatientUsage_paymentMethod;

    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[9].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[9].buffer = (char *)DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId;
    DBOP_VAR_ExecuteCreatePatientUsage_bindParams[9].buffer_length = strlen(DBOP_VAR_ExecuteCreatePatientUsage_noConstAuditId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreatePatientUsage_connect->stmt_create_patient_usage, DBOP_VAR_ExecuteCreatePatientUsage_bindParams)) {
        dzlog_error("[req: %s] Failed to bind create patient usage param: %s", DBOP_VAR_ExecuteCreatePatientUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreatePatientUsage_connect->stmt_create_patient_usage));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreatePatientUsage_connect->stmt_create_patient_usage)) {
        dzlog_error("[req: %s] Failed to execute create patient usage statement: %s", DBOP_VAR_ExecuteCreatePatientUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreatePatientUsage_connect->stmt_create_patient_usage));
        return -1;
    }

    dzlog_info("[req: %s] Successfully created patient usage record with ledger ID: %s", DBOP_VAR_ExecuteCreatePatientUsage_requestId, DBOP_VAR_ExecuteCreatePatientUsage_ledgerId);
    return 0;
}

// 新增患者使用记录的API接口
void DBOP_FUN_ApiCreatePatientUsage(struct evhttp_request *DBOP_VAR_ApiCreatePatientUsage_request, void *DBOP_VAR_ApiCreatePatientUsage_voidCfg) {
    AppConfig *DBOP_VAR_ApiCreatePatientUsage_cfg = (AppConfig *)DBOP_VAR_ApiCreatePatientUsage_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiCreatePatientUsage_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiCreatePatientUsage_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to ApiCreatePatientUsage.", DBOP_VAR_ApiCreatePatientUsage_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiCreatePatientUsage_request, DBOP_VAR_ApiCreatePatientUsage_cfg, DBOP_VAR_ApiCreatePatientUsage_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiCreatePatientUsage_request, DBOP_VAR_ApiCreatePatientUsage_requestId);
    if (!DBOP_VAR_ApiCreatePatientUsage_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "ledger_id");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonProjectId = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonUserId = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererUserId = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "sufferer_user_id");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererRealName = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "sufferer_real_name");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererUserName = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "sufferer_user_name");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonAmount = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "amount");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonNote = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "note");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonPaymentMethod = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "payment_method");
    json_t *DBOP_VAR_ApiCreatePatientUsage_dataJsonVerificationRecord = json_object_get(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll, "verification_record");

    if (!json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonLedgerId) ||
        !json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonProjectId) ||
        !json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererUserId) ||
        !json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererRealName) ||
        !json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererUserName) ||
        !json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonAmount) ||
        !json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonNote) ||
        !json_is_integer(DBOP_VAR_ApiCreatePatientUsage_dataJsonPaymentMethod) ||
        !json_is_string(DBOP_VAR_ApiCreatePatientUsage_dataJsonVerificationRecord)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting correct types for all fields", DBOP_VAR_ApiCreatePatientUsage_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreatePatientUsage_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiCreatePatientUsage_ledgerId = json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonLedgerId);
    const char *DBOP_VAR_ApiCreatePatientUsage_projectId = json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonProjectId);
    const char *DBOP_VAR_ApiCreatePatientUsage_userId = json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonUserId);
    const char *DBOP_VAR_ApiCreatePatientUsage_suffererUserId = json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererUserId);
    const char *DBOP_VAR_ApiCreatePatientUsage_suffererRealName = json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererRealName);
    const char *DBOP_VAR_ApiCreatePatientUsage_suffererUserName = json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonSuffererUserName);
    int DBOP_VAR_ApiCreatePatientUsage_amount = atoi(json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonAmount));
    const char *DBOP_VAR_ApiCreatePatientUsage_note = json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonNote);
    int DBOP_VAR_ApiCreatePatientUsage_paymentMethod = (int)json_integer_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonPaymentMethod);
    const char *DBOP_VAR_ApiCreatePatientUsage_verificationRecord = json_string_value(DBOP_VAR_ApiCreatePatientUsage_dataJsonVerificationRecord);

    dzlog_info("[req: %s] Executing database operation for ApiCreatePatientUsage: ledgerId=%s, projectId=%s, userId=%s, amount=%d", 
               DBOP_VAR_ApiCreatePatientUsage_requestId, DBOP_VAR_ApiCreatePatientUsage_ledgerId, DBOP_VAR_ApiCreatePatientUsage_projectId, DBOP_VAR_ApiCreatePatientUsage_userId, DBOP_VAR_ApiCreatePatientUsage_amount);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiCreatePatientUsage_mysqlConnect = DBOP_FUN_GetDatabaseConnection(DBOP_VAR_ApiCreatePatientUsage_cfg);
    int result = DBOP_FUN_ExecuteCreatePatientUsage(DBOP_VAR_ApiCreatePatientUsage_mysqlConnect, DBOP_VAR_ApiCreatePatientUsage_ledgerId, DBOP_VAR_ApiCreatePatientUsage_projectId, DBOP_VAR_ApiCreatePatientUsage_userId, DBOP_VAR_ApiCreatePatientUsage_suffererUserId, DBOP_VAR_ApiCreatePatientUsage_suffererRealName, DBOP_VAR_ApiCreatePatientUsage_suffererUserName, DBOP_VAR_ApiCreatePatientUsage_amount, DBOP_VAR_ApiCreatePatientUsage_note, DBOP_VAR_ApiCreatePatientUsage_paymentMethod, DBOP_VAR_ApiCreatePatientUsage_verificationRecord, DBOP_VAR_ApiCreatePatientUsage_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiCreatePatientUsage_request, result, DBOP_VAR_ApiCreatePatientUsage_requestId, "create patient usage");

    json_decref(DBOP_VAR_ApiCreatePatientUsage_dataJsonAll);
}

// ------------------------mysql新增患者使用记录api逻辑结束----------------------------

// ------------------------mysql患者使用记录设置处理失败状态api逻辑开始----------------------------

// 将患者使用记录状态设置为处理失败
int DBOP_FUN_ExecuteSetPatientUsageProcessFailed(DB_CONNECTION *DBOP_VAR_ExecuteSetPatientUsageProcessFailed_connect, const char *DBOP_VAR_ExecuteSetPatientUsageProcessFailed_ledgerId, const char *DBOP_VAR_ExecuteSetPatientUsageProcessFailed_requestId) {
    char *DBOP_VAR_ExecuteSetPatientUsageProcessFailed_noConstLedgerId = (char *)DBOP_VAR_ExecuteSetPatientUsageProcessFailed_ledgerId;

    MYSQL_BIND DBOP_VAR_ExecuteSetPatientUsageProcessFailed_bindParams[1];
    memset(DBOP_VAR_ExecuteSetPatientUsageProcessFailed_bindParams, 0, sizeof(DBOP_VAR_ExecuteSetPatientUsageProcessFailed_bindParams));

    DBOP_VAR_ExecuteSetPatientUsageProcessFailed_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteSetPatientUsageProcessFailed_bindParams[0].buffer = DBOP_VAR_ExecuteSetPatientUsageProcessFailed_noConstLedgerId;
    DBOP_VAR_ExecuteSetPatientUsageProcessFailed_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteSetPatientUsageProcessFailed_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteSetPatientUsageProcessFailed_connect->stmt_set_status_patient_usage_process_failed, DBOP_VAR_ExecuteSetPatientUsageProcessFailed_bindParams)) {
        dzlog_error("[req: %s] Failed to bind set patient usage process failed param: %s", DBOP_VAR_ExecuteSetPatientUsageProcessFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteSetPatientUsageProcessFailed_connect->stmt_set_status_patient_usage_process_failed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteSetPatientUsageProcessFailed_connect->stmt_set_status_patient_usage_process_failed)) {
        dzlog_error("[req: %s] Failed to execute set patient usage process failed statement: %s", DBOP_VAR_ExecuteSetPatientUsageProcessFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteSetPatientUsageProcessFailed_connect->stmt_set_status_patient_usage_process_failed));
        return -1;
    }

    my_ulonglong DBOP_VAR_ExecuteSetPatientUsageProcessFailed_affectedRows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteSetPatientUsageProcessFailed_connect->stmt_set_status_patient_usage_process_failed);
    if (DBOP_VAR_ExecuteSetPatientUsageProcessFailed_affectedRows == 0) {
        dzlog_warn("[req: %s] No patient usage record found with ledger ID: %s", DBOP_VAR_ExecuteSetPatientUsageProcessFailed_requestId, DBOP_VAR_ExecuteSetPatientUsageProcessFailed_ledgerId);
        return 1; // 记录不存在
    }

    dzlog_info("[req: %s] Successfully set patient usage process failed status for ledger ID: %s", DBOP_VAR_ExecuteSetPatientUsageProcessFailed_requestId, DBOP_VAR_ExecuteSetPatientUsageProcessFailed_ledgerId);
    return 0;
}



// 将患者使用记录状态设置为处理失败的API接口
void DBOP_FUN_ApiSetPatientUsageProcessFailed(struct evhttp_request *DBOP_VAR_ApiSetPatientUsageProcessFailed_request, void *DBOP_VAR_ApiSetPatientUsageProcessFailed_voidCfg) {
    AppConfig *DBOP_VAR_ApiSetPatientUsageProcessFailed_cfg = (AppConfig *)DBOP_VAR_ApiSetPatientUsageProcessFailed_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiSetPatientUsageProcessFailed_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiSetPatientUsageProcessFailed_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to set patient usage process failed.", DBOP_VAR_ApiSetPatientUsageProcessFailed_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiSetPatientUsageProcessFailed_request, DBOP_VAR_ApiSetPatientUsageProcessFailed_cfg, DBOP_VAR_ApiSetPatientUsageProcessFailed_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiSetPatientUsageProcessFailed_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiSetPatientUsageProcessFailed_request, DBOP_VAR_ApiSetPatientUsageProcessFailed_requestId);
    if (!DBOP_VAR_ApiSetPatientUsageProcessFailed_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiSetPatientUsageProcessFailed_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiSetPatientUsageProcessFailed_dataJsonAll, "ledger_id");
    if (!json_is_string(DBOP_VAR_ApiSetPatientUsageProcessFailed_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string for ledger_id", DBOP_VAR_ApiSetPatientUsageProcessFailed_requestId);
        evhttp_send_reply(DBOP_VAR_ApiSetPatientUsageProcessFailed_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiSetPatientUsageProcessFailed_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiSetPatientUsageProcessFailed_ledgerId = json_string_value(DBOP_VAR_ApiSetPatientUsageProcessFailed_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation to set patient usage process failed: ledgerId=%s", 
               DBOP_VAR_ApiSetPatientUsageProcessFailed_requestId, DBOP_VAR_ApiSetPatientUsageProcessFailed_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiSetPatientUsageProcessFailed_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiSetPatientUsageProcessFailed_cfg, 0);
    int result = DBOP_FUN_ExecuteSetPatientUsageProcessFailed(DBOP_VAR_ApiSetPatientUsageProcessFailed_mysqlConnect, DBOP_VAR_ApiSetPatientUsageProcessFailed_ledgerId, DBOP_VAR_ApiSetPatientUsageProcessFailed_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiSetPatientUsageProcessFailed_request, result, DBOP_VAR_ApiSetPatientUsageProcessFailed_requestId, "set patient usage process failed");

    json_decref(DBOP_VAR_ApiSetPatientUsageProcessFailed_dataJsonAll);
}

// ------------------------mysql患者使用记录设置处理失败状态api逻辑结束----------------------------

// ------------------------mysql患者使用记录设置发款中状态api逻辑开始----------------------------

// 将患者使用记录状态设置为发款中
int DBOP_FUN_ExecuteSetPatientUsagePaying(DB_CONNECTION *DBOP_VAR_ExecuteSetPatientUsagePaying_connect, const char *DBOP_VAR_ExecuteSetPatientUsagePaying_ledgerId, const char *DBOP_VAR_ExecuteSetPatientUsagePaying_requestId) {
    char *DBOP_VAR_ExecuteSetPatientUsagePaying_noConstLedgerId = (char *)DBOP_VAR_ExecuteSetPatientUsagePaying_ledgerId;

    MYSQL_BIND DBOP_VAR_ExecuteSetPatientUsagePaying_bindParams[1];
    memset(DBOP_VAR_ExecuteSetPatientUsagePaying_bindParams, 0, sizeof(DBOP_VAR_ExecuteSetPatientUsagePaying_bindParams));

    DBOP_VAR_ExecuteSetPatientUsagePaying_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteSetPatientUsagePaying_bindParams[0].buffer = DBOP_VAR_ExecuteSetPatientUsagePaying_noConstLedgerId;
    DBOP_VAR_ExecuteSetPatientUsagePaying_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteSetPatientUsagePaying_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteSetPatientUsagePaying_connect->stmt_set_status_patient_usage_paying, DBOP_VAR_ExecuteSetPatientUsagePaying_bindParams)) {
        dzlog_error("[req: %s] Failed to bind set patient usage paying param: %s", DBOP_VAR_ExecuteSetPatientUsagePaying_requestId, mysql_stmt_error(DBOP_VAR_ExecuteSetPatientUsagePaying_connect->stmt_set_status_patient_usage_paying));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteSetPatientUsagePaying_connect->stmt_set_status_patient_usage_paying)) {
        dzlog_error("[req: %s] Failed to execute set patient usage paying statement: %s", DBOP_VAR_ExecuteSetPatientUsagePaying_requestId, mysql_stmt_error(DBOP_VAR_ExecuteSetPatientUsagePaying_connect->stmt_set_status_patient_usage_paying));
        return -1;
    }

    my_ulonglong DBOP_VAR_ExecuteSetPatientUsagePaying_affectedRows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteSetPatientUsagePaying_connect->stmt_set_status_patient_usage_paying);
    if (DBOP_VAR_ExecuteSetPatientUsagePaying_affectedRows == 0) {
        dzlog_warn("[req: %s] No patient usage record found with ledger ID: %s", DBOP_VAR_ExecuteSetPatientUsagePaying_requestId, DBOP_VAR_ExecuteSetPatientUsagePaying_ledgerId);
        return 1; // 记录不存在
    }

    dzlog_info("[req: %s] Successfully set patient usage paying status for ledger ID: %s", DBOP_VAR_ExecuteSetPatientUsagePaying_requestId, DBOP_VAR_ExecuteSetPatientUsagePaying_ledgerId);
    return 0;
}


// 将患者使用记录状态设置为发款中的API接口
void DBOP_FUN_ApiSetPatientUsagePaying(struct evhttp_request *DBOP_VAR_ApiSetPatientUsagePaying_request, void *DBOP_VAR_ApiSetPatientUsagePaying_voidCfg) {
    AppConfig *DBOP_VAR_ApiSetPatientUsagePaying_cfg = (AppConfig *)DBOP_VAR_ApiSetPatientUsagePaying_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiSetPatientUsagePaying_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiSetPatientUsagePaying_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to set patient usage paying.", DBOP_VAR_ApiSetPatientUsagePaying_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiSetPatientUsagePaying_request, DBOP_VAR_ApiSetPatientUsagePaying_cfg, DBOP_VAR_ApiSetPatientUsagePaying_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiSetPatientUsagePaying_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiSetPatientUsagePaying_request, DBOP_VAR_ApiSetPatientUsagePaying_requestId);
    if (!DBOP_VAR_ApiSetPatientUsagePaying_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiSetPatientUsagePaying_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiSetPatientUsagePaying_dataJsonAll, "ledger_id");
    if (!json_is_string(DBOP_VAR_ApiSetPatientUsagePaying_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string for ledger_id", DBOP_VAR_ApiSetPatientUsagePaying_requestId);
        evhttp_send_reply(DBOP_VAR_ApiSetPatientUsagePaying_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiSetPatientUsagePaying_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiSetPatientUsagePaying_ledgerId = json_string_value(DBOP_VAR_ApiSetPatientUsagePaying_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation to set patient usage paying: ledgerId=%s", 
               DBOP_VAR_ApiSetPatientUsagePaying_requestId, DBOP_VAR_ApiSetPatientUsagePaying_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiSetPatientUsagePaying_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiSetPatientUsagePaying_cfg, 0);
    int result = DBOP_FUN_ExecuteSetPatientUsagePaying(DBOP_VAR_ApiSetPatientUsagePaying_mysqlConnect, DBOP_VAR_ApiSetPatientUsagePaying_ledgerId, DBOP_VAR_ApiSetPatientUsagePaying_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiSetPatientUsagePaying_request, result, DBOP_VAR_ApiSetPatientUsagePaying_requestId, "set patient usage paying");

    json_decref(DBOP_VAR_ApiSetPatientUsagePaying_dataJsonAll);
}

// ------------------------mysql患者使用记录设置发款中状态api逻辑结束----------------------------

// ------------------------mysql患者使用记录设置发款失败状态api逻辑开始----------------------------

// 将患者使用记录状态设置为发款失败
int DBOP_FUN_ExecuteSetPatientUsagePayFailed(DB_CONNECTION *DBOP_VAR_ExecuteSetPatientUsagePayFailed_connect, const char *DBOP_VAR_ExecuteSetPatientUsagePayFailed_ledgerId, const char *DBOP_VAR_ExecuteSetPatientUsagePayFailed_requestId) {
    char *DBOP_VAR_ExecuteSetPatientUsagePayFailed_noConstLedgerId = (char *)DBOP_VAR_ExecuteSetPatientUsagePayFailed_ledgerId;

    MYSQL_BIND DBOP_VAR_ExecuteSetPatientUsagePayFailed_bindParams[1];
    memset(DBOP_VAR_ExecuteSetPatientUsagePayFailed_bindParams, 0, sizeof(DBOP_VAR_ExecuteSetPatientUsagePayFailed_bindParams));

    DBOP_VAR_ExecuteSetPatientUsagePayFailed_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteSetPatientUsagePayFailed_bindParams[0].buffer = DBOP_VAR_ExecuteSetPatientUsagePayFailed_noConstLedgerId;
    DBOP_VAR_ExecuteSetPatientUsagePayFailed_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteSetPatientUsagePayFailed_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteSetPatientUsagePayFailed_connect->stmt_set_status_patient_usage_pay_failed, DBOP_VAR_ExecuteSetPatientUsagePayFailed_bindParams)) {
        dzlog_error("[req: %s] Failed to bind set patient usage pay failed param: %s", DBOP_VAR_ExecuteSetPatientUsagePayFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteSetPatientUsagePayFailed_connect->stmt_set_status_patient_usage_pay_failed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteSetPatientUsagePayFailed_connect->stmt_set_status_patient_usage_pay_failed)) {
        dzlog_error("[req: %s] Failed to execute set patient usage pay failed statement: %s", DBOP_VAR_ExecuteSetPatientUsagePayFailed_requestId, mysql_stmt_error(DBOP_VAR_ExecuteSetPatientUsagePayFailed_connect->stmt_set_status_patient_usage_pay_failed));
        return -1;
    }

    my_ulonglong DBOP_VAR_ExecuteSetPatientUsagePayFailed_affectedRows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteSetPatientUsagePayFailed_connect->stmt_set_status_patient_usage_pay_failed);
    if (DBOP_VAR_ExecuteSetPatientUsagePayFailed_affectedRows == 0) {
        dzlog_warn("[req: %s] No patient usage record found with ledger ID: %s", DBOP_VAR_ExecuteSetPatientUsagePayFailed_requestId, DBOP_VAR_ExecuteSetPatientUsagePayFailed_ledgerId);
        return 1; // 记录不存在
    }

    dzlog_info("[req: %s] Successfully set patient usage pay failed status for ledger ID: %s", DBOP_VAR_ExecuteSetPatientUsagePayFailed_requestId, DBOP_VAR_ExecuteSetPatientUsagePayFailed_ledgerId);
    return 0;
}



// 将患者使用记录状态设置为发款失败的API接口
void DBOP_FUN_ApiSetPatientUsagePayFailed(struct evhttp_request *DBOP_VAR_ApiSetPatientUsagePayFailed_request, void *DBOP_VAR_ApiSetPatientUsagePayFailed_voidCfg) {
    AppConfig *DBOP_VAR_ApiSetPatientUsagePayFailed_cfg = (AppConfig *)DBOP_VAR_ApiSetPatientUsagePayFailed_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiSetPatientUsagePayFailed_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiSetPatientUsagePayFailed_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to set patient usage pay failed.", DBOP_VAR_ApiSetPatientUsagePayFailed_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiSetPatientUsagePayFailed_request, DBOP_VAR_ApiSetPatientUsagePayFailed_cfg, DBOP_VAR_ApiSetPatientUsagePayFailed_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiSetPatientUsagePayFailed_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiSetPatientUsagePayFailed_request, DBOP_VAR_ApiSetPatientUsagePayFailed_requestId);
    if (!DBOP_VAR_ApiSetPatientUsagePayFailed_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiSetPatientUsagePayFailed_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiSetPatientUsagePayFailed_dataJsonAll, "ledger_id");
    if (!json_is_string(DBOP_VAR_ApiSetPatientUsagePayFailed_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string for ledger_id", DBOP_VAR_ApiSetPatientUsagePayFailed_requestId);
        evhttp_send_reply(DBOP_VAR_ApiSetPatientUsagePayFailed_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiSetPatientUsagePayFailed_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiSetPatientUsagePayFailed_ledgerId = json_string_value(DBOP_VAR_ApiSetPatientUsagePayFailed_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation to set patient usage pay failed: ledgerId=%s", 
               DBOP_VAR_ApiSetPatientUsagePayFailed_requestId, DBOP_VAR_ApiSetPatientUsagePayFailed_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiSetPatientUsagePayFailed_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiSetPatientUsagePayFailed_cfg, 0);
    int result = DBOP_FUN_ExecuteSetPatientUsagePayFailed(DBOP_VAR_ApiSetPatientUsagePayFailed_mysqlConnect, DBOP_VAR_ApiSetPatientUsagePayFailed_ledgerId, DBOP_VAR_ApiSetPatientUsagePayFailed_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiSetPatientUsagePayFailed_request, result, DBOP_VAR_ApiSetPatientUsagePayFailed_requestId, "set patient usage pay failed");

    json_decref(DBOP_VAR_ApiSetPatientUsagePayFailed_dataJsonAll);
}

// ------------------------mysql患者使用记录设置发款失败状态api逻辑结束----------------------------

// ------------------------mysql患者使用记录设置完成状态api逻辑开始----------------------------

// 将患者使用记录状态设置为完成
int DBOP_FUN_ExecuteSetPatientUsageCompleted(DB_CONNECTION *DBOP_VAR_ExecuteSetPatientUsageCompleted_connect, const char *DBOP_VAR_ExecuteSetPatientUsageCompleted_ledgerId, const char *DBOP_VAR_ExecuteSetPatientUsageCompleted_requestId) {
    char *DBOP_VAR_ExecuteSetPatientUsageCompleted_noConstLedgerId = (char *)DBOP_VAR_ExecuteSetPatientUsageCompleted_ledgerId;

    MYSQL_BIND DBOP_VAR_ExecuteSetPatientUsageCompleted_bindParams[1];
    memset(DBOP_VAR_ExecuteSetPatientUsageCompleted_bindParams, 0, sizeof(DBOP_VAR_ExecuteSetPatientUsageCompleted_bindParams));

    DBOP_VAR_ExecuteSetPatientUsageCompleted_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteSetPatientUsageCompleted_bindParams[0].buffer = DBOP_VAR_ExecuteSetPatientUsageCompleted_noConstLedgerId;
    DBOP_VAR_ExecuteSetPatientUsageCompleted_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteSetPatientUsageCompleted_noConstLedgerId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteSetPatientUsageCompleted_connect->stmt_set_status_patient_usage_completed, DBOP_VAR_ExecuteSetPatientUsageCompleted_bindParams)) {
        dzlog_error("[req: %s] Failed to bind set patient usage completed param: %s", DBOP_VAR_ExecuteSetPatientUsageCompleted_requestId, mysql_stmt_error(DBOP_VAR_ExecuteSetPatientUsageCompleted_connect->stmt_set_status_patient_usage_completed));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteSetPatientUsageCompleted_connect->stmt_set_status_patient_usage_completed)) {
        dzlog_error("[req: %s] Failed to execute set patient usage completed statement: %s", DBOP_VAR_ExecuteSetPatientUsageCompleted_requestId, mysql_stmt_error(DBOP_VAR_ExecuteSetPatientUsageCompleted_connect->stmt_set_status_patient_usage_completed));
        return -1;
    }

    my_ulonglong DBOP_VAR_ExecuteSetPatientUsageCompleted_affectedRows = mysql_stmt_affected_rows(DBOP_VAR_ExecuteSetPatientUsageCompleted_connect->stmt_set_status_patient_usage_completed);
    if (DBOP_VAR_ExecuteSetPatientUsageCompleted_affectedRows == 0) {
        dzlog_warn("[req: %s] No patient usage record found with ledger ID: %s", DBOP_VAR_ExecuteSetPatientUsageCompleted_requestId, DBOP_VAR_ExecuteSetPatientUsageCompleted_ledgerId);
        return 1; // 记录不存在
    }

    dzlog_info("[req: %s] Successfully set patient usage completed status for ledger ID: %s", DBOP_VAR_ExecuteSetPatientUsageCompleted_requestId, DBOP_VAR_ExecuteSetPatientUsageCompleted_ledgerId);
    return 0;
}



// 将患者使用记录状态设置为完成的API接口
void DBOP_FUN_ApiSetPatientUsageCompleted(struct evhttp_request *DBOP_VAR_ApiSetPatientUsageCompleted_request, void *DBOP_VAR_ApiSetPatientUsageCompleted_voidCfg) {
    AppConfig *DBOP_VAR_ApiSetPatientUsageCompleted_cfg = (AppConfig *)DBOP_VAR_ApiSetPatientUsageCompleted_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiSetPatientUsageCompleted_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiSetPatientUsageCompleted_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to set patient usage completed.", DBOP_VAR_ApiSetPatientUsageCompleted_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiSetPatientUsageCompleted_request, DBOP_VAR_ApiSetPatientUsageCompleted_cfg, DBOP_VAR_ApiSetPatientUsageCompleted_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiSetPatientUsageCompleted_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiSetPatientUsageCompleted_request, DBOP_VAR_ApiSetPatientUsageCompleted_requestId);
    if (!DBOP_VAR_ApiSetPatientUsageCompleted_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiSetPatientUsageCompleted_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiSetPatientUsageCompleted_dataJsonAll, "ledger_id");
    if (!json_is_string(DBOP_VAR_ApiSetPatientUsageCompleted_dataJsonLedgerId)) {
        dzlog_error("[req: %s] Invalid JSON data received. Expecting string for ledger_id", DBOP_VAR_ApiSetPatientUsageCompleted_requestId);
        evhttp_send_reply(DBOP_VAR_ApiSetPatientUsageCompleted_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiSetPatientUsageCompleted_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiSetPatientUsageCompleted_ledgerId = json_string_value(DBOP_VAR_ApiSetPatientUsageCompleted_dataJsonLedgerId);

    dzlog_info("[req: %s] Executing database operation to set patient usage completed: ledgerId=%s", 
               DBOP_VAR_ApiSetPatientUsageCompleted_requestId, DBOP_VAR_ApiSetPatientUsageCompleted_ledgerId);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiSetPatientUsageCompleted_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiSetPatientUsageCompleted_cfg, 0);
    int result = DBOP_FUN_ExecuteSetPatientUsageCompleted(DBOP_VAR_ApiSetPatientUsageCompleted_mysqlConnect, DBOP_VAR_ApiSetPatientUsageCompleted_ledgerId, DBOP_VAR_ApiSetPatientUsageCompleted_requestId);

    // 发送响应
    DBOP_FUN_SendStandardResponse(DBOP_VAR_ApiSetPatientUsageCompleted_request, result, DBOP_VAR_ApiSetPatientUsageCompleted_requestId, "set patient usage completed");

    json_decref(DBOP_VAR_ApiSetPatientUsageCompleted_dataJsonAll);
}

// ------------------------mysql患者使用记录设置完成状态api逻辑结束----------------------------

// ------------------------mysql新增应急使用记录api逻辑开始----------------------------

// 新增应急使用记录
int DBOP_FUN_ExecuteCreateEmergencyUsage(DB_CONNECTION *DBOP_VAR_ExecuteCreateEmergencyUsage_connect, const char *DBOP_VAR_ExecuteCreateEmergencyUsage_ledgerId, const char *DBOP_VAR_ExecuteCreateEmergencyUsage_projectId, const char *DBOP_VAR_ExecuteCreateEmergencyUsage_userId, const char *DBOP_VAR_ExecuteCreateEmergencyUsage_suffererUserId, const char *DBOP_VAR_ExecuteCreateEmergencyUsage_suffererRealName, const char *DBOP_VAR_ExecuteCreateEmergencyUsage_suffererUserName, int DBOP_VAR_ExecuteCreateEmergencyUsage_amount, const char *DBOP_VAR_ExecuteCreateEmergencyUsage_note, int DBOP_VAR_ExecuteCreateEmergencyUsage_paymentMethod, const char *DBOP_VAR_ExecuteCreateEmergencyUsage_requestId) {
    // 首先检查该项目和患者是否还有其他未核销的应急使用账单
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_noConstProjectId = (char *)DBOP_VAR_ExecuteCreateEmergencyUsage_projectId;
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererUserId = (char *)DBOP_VAR_ExecuteCreateEmergencyUsage_suffererUserId;

    MYSQL_BIND DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams[2];
    memset(DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams, 0, sizeof(DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams));

    DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams[0].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstProjectId;
    DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstProjectId);

    DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams[1].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererUserId;
    DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererUserId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage, DBOP_VAR_ExecuteCreateEmergencyUsage_checkBindParams)) {
        dzlog_error("[req: %s] Failed to bind check pending emergency usage params: %s", DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage)) {
        dzlog_error("[req: %s] Failed to execute check pending emergency usage statement: %s", DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage));
        return -1;
    }

    // 绑定结果
    long long DBOP_VAR_ExecuteCreateEmergencyUsage_pendingCount = 0;
    MYSQL_BIND DBOP_VAR_ExecuteCreateEmergencyUsage_resultBind[1];
    memset(DBOP_VAR_ExecuteCreateEmergencyUsage_resultBind, 0, sizeof(DBOP_VAR_ExecuteCreateEmergencyUsage_resultBind));

    DBOP_VAR_ExecuteCreateEmergencyUsage_resultBind[0].buffer_type = MYSQL_TYPE_LONGLONG;
    DBOP_VAR_ExecuteCreateEmergencyUsage_resultBind[0].buffer = &DBOP_VAR_ExecuteCreateEmergencyUsage_pendingCount;

    if (mysql_stmt_bind_result(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage, DBOP_VAR_ExecuteCreateEmergencyUsage_resultBind)) {
        dzlog_error("[req: %s] Failed to bind check pending emergency usage result: %s", DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage));
        return -1;
    }

    if (mysql_stmt_fetch(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage) == 0) {
        if (DBOP_VAR_ExecuteCreateEmergencyUsage_pendingCount > 0) {
            dzlog_warn("[req: %s] Found %lld pending emergency usage records for project %s and sufferer %s", 
                      DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, DBOP_VAR_ExecuteCreateEmergencyUsage_pendingCount, 
                      DBOP_VAR_ExecuteCreateEmergencyUsage_projectId, DBOP_VAR_ExecuteCreateEmergencyUsage_suffererUserId);
            mysql_stmt_free_result(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage);
            return 2; // 返回特殊错误码表示存在未核销的应急使用账单
        }
    }

    mysql_stmt_free_result(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_check_pending_emergency_usage);
    
    // 检查通过，开始创建志愿者审计记录
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_noConstLedgerId = (char *)DBOP_VAR_ExecuteCreateEmergencyUsage_ledgerId;
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_emptyVerificationRecord = "";

    MYSQL_BIND DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams[2];
    memset(DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams, 0, sizeof(DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams));

    DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams[0].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstLedgerId;
    DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstLedgerId);

    DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams[1].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_emptyVerificationRecord;
    DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_emptyVerificationRecord);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_create_volunteer_audit, DBOP_VAR_ExecuteCreateEmergencyUsage_auditBindParams)) {
        dzlog_error("[req: %s] Failed to bind create volunteer audit params: %s", DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_create_volunteer_audit));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_create_volunteer_audit)) {
        dzlog_error("[req: %s] Failed to execute create volunteer audit statement: %s", DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_create_volunteer_audit));
        return -1;
    }

    // 然后创建应急使用记录
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_noConstUserId = (char *)DBOP_VAR_ExecuteCreateEmergencyUsage_userId;
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererRealName = (char *)DBOP_VAR_ExecuteCreateEmergencyUsage_suffererRealName;
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererUserName = (char *)DBOP_VAR_ExecuteCreateEmergencyUsage_suffererUserName;
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_noConstNote = (char *)DBOP_VAR_ExecuteCreateEmergencyUsage_note;
    char *DBOP_VAR_ExecuteCreateEmergencyUsage_noConstVolunteerAuditId = (char *)DBOP_VAR_ExecuteCreateEmergencyUsage_ledgerId; // 使用相同的ID

    MYSQL_BIND DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[10];
    memset(DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams, 0, sizeof(DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams));

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[0].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstLedgerId;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstLedgerId);

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[1].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstProjectId;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstProjectId);

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[2].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstUserId;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[2].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstUserId);

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[3].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[3].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererUserId;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[3].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererUserId);

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[4].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[4].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererRealName;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[4].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererRealName);

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[5].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[5].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererUserName;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[5].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstSuffererUserName);

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[6].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[6].buffer = &DBOP_VAR_ExecuteCreateEmergencyUsage_amount;

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[7].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[7].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstNote;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[7].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstNote);

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[8].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[8].buffer = &DBOP_VAR_ExecuteCreateEmergencyUsage_paymentMethod;

    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[9].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[9].buffer = DBOP_VAR_ExecuteCreateEmergencyUsage_noConstVolunteerAuditId;
    DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams[9].buffer_length = strlen(DBOP_VAR_ExecuteCreateEmergencyUsage_noConstVolunteerAuditId);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_create_emergency_usage, DBOP_VAR_ExecuteCreateEmergencyUsage_bindParams)) {
        dzlog_error("[req: %s] Failed to bind create emergency usage params: %s", DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_create_emergency_usage));
        return -1;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_create_emergency_usage)) {
        dzlog_error("[req: %s] Failed to execute create emergency usage statement: %s", DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateEmergencyUsage_connect->stmt_create_emergency_usage));
        return -1;
    }

    dzlog_info("[req: %s] Successfully created emergency usage record with ledger ID: %s", DBOP_VAR_ExecuteCreateEmergencyUsage_requestId, DBOP_VAR_ExecuteCreateEmergencyUsage_ledgerId);
    return 0;
}

// 新增应急使用记录的API接口
void DBOP_FUN_ApiCreateEmergencyUsage(struct evhttp_request *DBOP_VAR_ApiCreateEmergencyUsage_request, void *DBOP_VAR_ApiCreateEmergencyUsage_voidCfg) {
    AppConfig *DBOP_VAR_ApiCreateEmergencyUsage_cfg = (AppConfig *)DBOP_VAR_ApiCreateEmergencyUsage_voidCfg;
    char uuid_str[37];
    const char *DBOP_VAR_ApiCreateEmergencyUsage_requestId = DBOP_FUN_GetOrGenerateRequestId(DBOP_VAR_ApiCreateEmergencyUsage_request, uuid_str);
    
    dzlog_info("[req: %s] Processing API request to create emergency usage.", DBOP_VAR_ApiCreateEmergencyUsage_requestId);

    // 请求鉴权
    if (!DBOP_FUN_HandleAuthentication(DBOP_VAR_ApiCreateEmergencyUsage_request, DBOP_VAR_ApiCreateEmergencyUsage_cfg, DBOP_VAR_ApiCreateEmergencyUsage_requestId)) {
        return;
    }
    
    // 解析POST数据
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll = DBOP_FUN_ParsePostDataToJson(DBOP_VAR_ApiCreateEmergencyUsage_request, DBOP_VAR_ApiCreateEmergencyUsage_requestId);
    if (!DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll) {
        return;
    }

    // 验证JSON字段
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonLedgerId = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "ledger_id");
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonProjectId = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonUserId = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "user_id");
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererUserId = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "sufferer_user_id");
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererRealName = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "sufferer_real_name");
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererUserName = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "sufferer_user_name");
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAmount = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "amount");
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonNote = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "note");
    json_t *DBOP_VAR_ApiCreateEmergencyUsage_dataJsonPaymentMethod = json_object_get(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll, "payment_method");

    if (!json_is_string(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonLedgerId) ||
        !json_is_string(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonProjectId) ||
        !json_is_string(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonUserId) ||
        !json_is_string(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererUserId) ||
        !json_is_string(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererRealName) ||
        !json_is_string(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererUserName) ||
        !json_is_integer(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAmount) ||
        !json_is_string(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonNote) ||
        !json_is_integer(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonPaymentMethod)) {
        dzlog_error("[req: %s] Invalid JSON data received. Missing or invalid required fields", DBOP_VAR_ApiCreateEmergencyUsage_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreateEmergencyUsage_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll);
        return;
    }

    const char *DBOP_VAR_ApiCreateEmergencyUsage_ledgerId = json_string_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonLedgerId);
    const char *DBOP_VAR_ApiCreateEmergencyUsage_projectId = json_string_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonProjectId);
    const char *DBOP_VAR_ApiCreateEmergencyUsage_userId = json_string_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonUserId);
    const char *DBOP_VAR_ApiCreateEmergencyUsage_suffererUserId = json_string_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererUserId);
    const char *DBOP_VAR_ApiCreateEmergencyUsage_suffererRealName = json_string_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererRealName);
    const char *DBOP_VAR_ApiCreateEmergencyUsage_suffererUserName = json_string_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonSuffererUserName);
    int DBOP_VAR_ApiCreateEmergencyUsage_amount = json_integer_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAmount);
    const char *DBOP_VAR_ApiCreateEmergencyUsage_note = json_string_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonNote);
    int DBOP_VAR_ApiCreateEmergencyUsage_paymentMethod = json_integer_value(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonPaymentMethod);

    dzlog_info("[req: %s] Executing database operation for ApiCreateEmergencyUsage: ledgerId=%s, projectId=%s, userId=%s, amount=%d", 
               DBOP_VAR_ApiCreateEmergencyUsage_requestId, DBOP_VAR_ApiCreateEmergencyUsage_ledgerId, DBOP_VAR_ApiCreateEmergencyUsage_projectId, DBOP_VAR_ApiCreateEmergencyUsage_userId, DBOP_VAR_ApiCreateEmergencyUsage_amount);

    // 获取数据库连接并执行操作
    DB_CONNECTION *DBOP_VAR_ApiCreateEmergencyUsage_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiCreateEmergencyUsage_cfg, 0);
    int result = DBOP_FUN_ExecuteCreateEmergencyUsage(DBOP_VAR_ApiCreateEmergencyUsage_mysqlConnect, DBOP_VAR_ApiCreateEmergencyUsage_ledgerId, DBOP_VAR_ApiCreateEmergencyUsage_projectId, DBOP_VAR_ApiCreateEmergencyUsage_userId, DBOP_VAR_ApiCreateEmergencyUsage_suffererUserId, DBOP_VAR_ApiCreateEmergencyUsage_suffererRealName, DBOP_VAR_ApiCreateEmergencyUsage_suffererUserName, DBOP_VAR_ApiCreateEmergencyUsage_amount, DBOP_VAR_ApiCreateEmergencyUsage_note, DBOP_VAR_ApiCreateEmergencyUsage_paymentMethod, DBOP_VAR_ApiCreateEmergencyUsage_requestId);

    // 发送响应
    if (result == 0) {
        evhttp_send_reply(DBOP_VAR_ApiCreateEmergencyUsage_request, HTTP_OK, "OK", NULL);
        dzlog_info("[req: %s] Successfully created emergency usage record, returning 200", DBOP_VAR_ApiCreateEmergencyUsage_requestId);
    } else if (result == 2) {
        evhttp_send_reply(DBOP_VAR_ApiCreateEmergencyUsage_request, 400, "存在尚未核销的应急使用账单，请核销完成后才可发起下一个应急使用单", NULL);
        dzlog_warn("[req: %s] Cannot create emergency usage record: pending emergency usage bills exist, returning 400", DBOP_VAR_ApiCreateEmergencyUsage_requestId);
    } else {
        evhttp_send_reply(DBOP_VAR_ApiCreateEmergencyUsage_request, HTTP_INTERNAL, "Internal Server Error", NULL);
        dzlog_error("[req: %s] Failed to create emergency usage record, returning 500", DBOP_VAR_ApiCreateEmergencyUsage_requestId);
    }

    json_decref(DBOP_VAR_ApiCreateEmergencyUsage_dataJsonAll);
}

// ------------------------mysql新增应急使用记录api逻辑结束----------------------------


int main() { 
    AppConfig DBOP_VAR_Main_cfg = DBOP_FUN_MainConfigParse("config/config.yaml"); //初始化结构体
    struct event_base *DBOP_VAR_Main_eventBase = event_base_new();
    struct evhttp *DBOP_VAR_Main_httpServer = evhttp_new(DBOP_VAR_Main_eventBase);
    
    // 使用更安全的随机数种子初始化
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand((unsigned int)(ts.tv_nsec ^ ts.tv_sec));

    // 从配置获取Redis连接池大小，默认为10
    int redisPoolSize = 10;
    if (DBOP_VAR_Main_cfg.DBOP_GLV_redisPoolSize != NULL) {
        redisPoolSize = atoi(DBOP_VAR_Main_cfg.DBOP_GLV_redisPoolSize);
        if (redisPoolSize <= 0) {
            redisPoolSize = 10; // 如果配置无效，使用默认值
        }
    }
    
    // 保存实际的 Redis 连接池大小
    DBOP_GLV_actualRedisPoolSize = redisPoolSize;
    
    // 加锁保护Redis连接池初始化
    pthread_mutex_lock(&DBOP_GLV_redisConnection_mutex);
    DBOP_GLV_redisConnectPool = DBOP_FUN_InitializeRedisPool(&DBOP_VAR_Main_cfg, redisPoolSize);
    pthread_mutex_unlock(&DBOP_GLV_redisConnection_mutex);
    
    dzlog_info("Initialized Redis connection pool with %d connections", redisPoolSize);
    
    DBOP_FUN_InitLogging(&DBOP_VAR_Main_cfg);
    
    DBOP_FUN_InitializeConnPool(&DBOP_VAR_Main_cfg);

    // 路径路由
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/user/add", DBOP_FUN_ApiAddUser, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/auth/get_service_passwd", DBOP_FUN_ApiGetServicePasswd, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/project/create", DBOP_FUN_ApiCreateProject, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/user/update_login_time", DBOP_FUN_ApiUpdateLoginTime, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/user/update_info", DBOP_FUN_ApiUpdateUserInfo, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/user/update_phone", DBOP_FUN_ApiUpdateUserPhone, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/user/update_weixin", DBOP_FUN_ApiUpdateUserWeixinId, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/user/update_permission", DBOP_FUN_ApiUpdateUserPermission, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/user/update_password", DBOP_FUN_ApiUpdateUserPasswd, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/project/update_verification_record", DBOP_FUN_ApiUpdateVerificationRecord, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/project/update_status", DBOP_FUN_ApiUpdateProjectStatus, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/project/update_pathography", DBOP_FUN_ApiUpdateProjectPathography, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/project/update_volunteer", DBOP_FUN_ApiUpdateProjectVolunteer, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/create_donation", DBOP_FUN_ApiMakeDonation, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_payment_failed", DBOP_FUN_ApiMarkPaymentFailed, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_payment_processing", DBOP_FUN_ApiMarkPaymentProcessing, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_payment_process_failed", DBOP_FUN_ApiMarkPaymentProcessFailed, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_payment_refunding", DBOP_FUN_ApiMarkPaymentRefunding, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_payment_refund_failed", DBOP_FUN_ApiMarkPaymentRefundFailed, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_payment_refund_completed", DBOP_FUN_ApiMarkPaymentRefundCompleted, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_payment_completed", DBOP_FUN_ApiMarkPaymentCompleted, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/create_refund_pending_selection", DBOP_FUN_ApiCreateRefundPendingSelection, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_type_refund", DBOP_FUN_ApiMarkRefundSelected, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_refund_failed", DBOP_FUN_ApiMarkRefundFailed, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_refund_completed", DBOP_FUN_ApiMarkRefundCompleted, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_type_emergency_pool", DBOP_FUN_ApiMarkEmergencyPool, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/create_patient_usage", DBOP_FUN_ApiCreatePatientUsage, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_patient_usage_process_failed", DBOP_FUN_ApiSetPatientUsageProcessFailed, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_patient_usage_paying", DBOP_FUN_ApiSetPatientUsagePaying, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_patient_usage_pay_failed", DBOP_FUN_ApiSetPatientUsagePayFailed, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/set_status_patient_usage_completed", DBOP_FUN_ApiSetPatientUsageCompleted, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/ledger/create_emergency_usage", DBOP_FUN_ApiCreateEmergencyUsage, &DBOP_VAR_Main_cfg);

    // 绑定到 0.0.0.0:DBOP_GLV_serverPort
    if (evhttp_bind_socket(DBOP_VAR_Main_httpServer, "0.0.0.0", atoi(DBOP_VAR_Main_cfg.DBOP_GLV_serverPort)) != 0) {
        dzlog_error("Failed to bind to port %s", DBOP_VAR_Main_cfg.DBOP_GLV_serverPort);
        return 1;
    }

    // 启动事件循环
    event_base_dispatch(DBOP_VAR_Main_eventBase);

    // 清理资源
    DBOP_FUN_DestroyConnPool();
    DBOP_FUN_DestroyRedisPool(DBOP_GLV_redisConnectPool);
    DBOP_FUN_FreeConfig(&DBOP_VAR_Main_cfg);
    evhttp_free(DBOP_VAR_Main_httpServer);
    event_base_free(DBOP_VAR_Main_eventBase);
    
    //关闭zlog
    zlog_fini();
    return 0;
}
