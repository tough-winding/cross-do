// 编译命令： gcc -o database_operation.exe database_operation.c -levent -ljansson -lzlog -lyaml -luuid -L/usr/lib64/mysql -lhiredis -lmysqlclient -lpthread -std=c99 -O3

// 整体目的：控制数据库的增删改查操作，均由该服务提供，对外发布socket服务
/*
    访问接口样例及返回体样例
    REQUEST：   查询service对应的service密码
                curl -X POST "http://服务ip:服务端口/get_service_passwd" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"service_name":"分配的服务名"}'
                curl -X POST "http://192.168.1.10:1900/get_service_passwd" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"service_username":"ftp"}'
    200RETURN： {"ServiceName":"ftp", "ServicePassword":"anotherlongpassword987654"}  
    REQUEST：   在user表中添加用户
                curl -X POST "http://服务ip:服务端口/add_user" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "user_name":"用户名", "user_passwd":"用户密码字符串", "user_permission":账号分类数字, "phone_number":"用户手机号"}'
                curl -X POST "http://192.168.1.10:1900/add_user" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d {"user_id":"d7aa0e59-9be5-4c76-9bf1-ee937791b8a8", "user_name":"两茫茫", "user_passwd":"qwe1j31l23nubadbuiy13", "user_permission":3, "phone_number":"18212312310"}'
    200RETURN： 什么都不返回
    REQUEST:    更新最后一次登录时间
                curl -X POST "http://服务ip:服务端口/update_login_time" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID"}'
                curl -X POST "http://192.168.1.10:1900/update_login_time" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db"}'
    200RETURN： 什么都不返回
    REQUEST:    更新用户个性化信息
                curl -X POST "http://服务ip:服务端口/update_user_info" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "user_name":"用户名", "avatar":"用户头像路径", "personal_note": "个人说明"}'
                curl -X POST "http://192.168.1.10:1900/update_user_info" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "user_name":"李通", "avatar":"https://www.test.com/img/flexible/logo/pc/result.png", "personal_note": "生死两茫茫"}'
    200RETURN： 什么都不返回
    REQUEST:    更新用户手机号
                curl -X POST "http://服务ip:服务端口/update_user_phone" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "phone_number":"用户手机号"}'
                curl -X POST "http://192.168.1.10:1900/update_user_phone" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "phone_number": "19212345678"}'
    200RETURN： 什么都不返回
    REQUEST:    更新用户微信ID
                curl -X POST "http://服务ip:服务端口/update_user_phone" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "weixin_openid":"用户微信OpenID"}'
                curl -X POST "http://192.168.1.10:1900/update_user_phone" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "weixin_openid": "4fcb6538-9b7e-4f25-8a01-47016843b7a6"}'
    200RETURN： 什么都不返回
    REQUEST:    更新用户权限组
                curl -X POST "http://服务ip:服务端口/update_user_permission" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "user_permission":账号分类数字}'
                curl -X POST "http://192.168.1.10:1900/update_user_permission" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "user_permission": 1}'
    200RETURN： 什么都不返回
    REQUEST:    修改用户密码
                curl -X POST "http://服务ip:服务端口/update_user_passwd" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user_id":"用户ID", "passwd":"用户密码字符串"}'
                curl -X POST "http://192.168.1.10:1900/update_user_passwd" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "passwd": "kojiqjwe1312uj2"}'
    200RETURN： 什么都不返回
    REQUEST:    创建项目
                curl -X POST "http://服务ip:服务端口/create_project" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "user_id":"用户ID", "user_name":"用户名", "user_age":"用户年龄", "real_name":"用户姓名"}'
                curl -X POST "http://192.168.1.10:1900/create_project" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "user_id":"b8a1a6f9-739d-4a5d-9183-1d7ad9e1f6db", "user_name":"李通", "user_age":67, "real_name":"李通达"}'
    200RETURN： 什么都不返回
    REQUEST:    更新验证记录
                curl -X POST "http://服务ip:服务端口/update_verification_record" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "update_verification_record":"消息json"}'
                curl -X POST "http://192.168.1.10:1900/update_verification_record" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "update_verification_record":"[{"patient":{"我在遥望":"请志愿者看下这次的资料合格了吗？","attachment":["https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/病历初稿.pdf","https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/检查报告.jpg"],"time":"2025-05-27T14:00:00+08:00"},"volunteer":{"bceac":"您的资料还存在以下问题：病历不完整，没有医生签字证明。请补充好之后再提交一下。","attachment":[],"time":"2025-05-27T14:15:00+08:00"}},{"patient":{"我在遥望":"我又调整了一下，您再看下呢？","attachment":["https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/补充病历.pdf","https://minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/医生签字页.png"],"time":"2025-05-27T15:00:00+08:00"},"volunteer":{"bceac":"这次可以了，给您通过了。","attachment":[],"time":"2025-05-27T15:10:00+08:00"}}]"}'
    200RETURN： 什么都不返回
    REQUEST:    更新项目状态
                curl -X POST "http://服务ip:服务端口/update_projest_status" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "projest_status":项目状态码}'
                curl -X POST "http://192.168.1.10:1900/update_projest_status" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "projest_status":1}'
    200RETURN： 什么都不返回
    REQUEST:    更新病情记录
                curl -X POST "http://服务ip:服务端口/update_projest_pathography" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "pathography":"记录JSON"}'
                curl -X POST "http://192.168.1.10:1900/update_projest_pathography" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "pathography":"[{"time":"2020-01-05 14:03:01","content":"刚刚确诊为***，心情很是沉重，医生说需要******来治疗","attachment":["minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/20200105140301-1.jpg","minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/20200105140301-2.jpg"]},{"time":"2020-01-15 12:25:31","content":"这一周多的治疗效果好多了，现在又有点希望了","attachment":["minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/20200115122531-1.jpg","minio.cross.com/550e8400-e29b-41d4-a716-446655440000/f47ac10b-58cc-4372-a567-0e02b2c3d479/20200115122531-2.jpg"]}]"}'
    200RETURN： 什么都不返回
    REQUEST:    更新项目负责志愿者ID
                curl -X POST "http://服务ip:服务端口/update_project_volunteer" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"项目ID", "volunteer_id":"志愿者ID"}'
                curl -X POST "http://192.168.1.10:1900/update_project_volunteer" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"f47ac10b-58cc-4372-a567-0e02b2c3d479", "volunteer_id":"8f76fa9c-18d2-41a9-bdd6-6c8e4c489872"}'
    200RETURN： 什么都不返回

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
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/add_user", DBOP_FUN_ApiAddUser, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/get_service_passwd", DBOP_FUN_ApiGetServicePasswd, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/create_project", DBOP_FUN_ApiCreateProject, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_login_time", DBOP_FUN_ApiUpdateLoginTime, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_user_info", DBOP_FUN_ApiUpdateUserInfo, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_user_phone", DBOP_FUN_ApiUpdateUserPhone, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_user_weixinid", DBOP_FUN_ApiUpdateUserWeixinId, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_user_permission", DBOP_FUN_ApiUpdateUserPermission, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_user_passwd", DBOP_FUN_ApiUpdateUserPasswd, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_verification_record", DBOP_FUN_ApiUpdateVerificationRecord, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_projest_status", DBOP_FUN_ApiUpdateProjectStatus, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_projest_pathography", DBOP_FUN_ApiUpdateProjectPathography, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/update_project_volunteer", DBOP_FUN_ApiUpdateProjectVolunteer, &DBOP_VAR_Main_cfg);

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
