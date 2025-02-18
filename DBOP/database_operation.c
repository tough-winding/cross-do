// 编译命令： gcc -o database_operation.exe database_operation.c -levent -ljansson -lzlog -lyaml -luuid -L/usr/lib64/mysql -lhiredis -lmysqlclient -lpthread -std=c99 -O3

// 整体目的：控制数据库的增删改查操作，均由该服务提供，对外发布socket服务
/*
    访问接口样例及返回体样例
    REQUEST：   查询service对应的service密码
                curl -X POST "http://服务ip:服务端口/get_service_passwd" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"service_name":"分配的服务名"}'
                curl -X POST "http://192.168.1.10:1900/get_service_passwd" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"service_username":"ftp"}'
    200RETURN： {"ServiceName":"ftp", "ServicePassword":"anotherlongpassword987654"}  
    REQUEST：   在user表中添加账户（测试用接口，没啥用）
                curl -X POST "http://服务ip:服务端口/add_user_from_pool" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"user":"用户名", "password":"密码"}'
                curl -X POST "http://192.168.1.10:1900/add_user_from_pool" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"user":"test_user1", "password":"12345"}'
    200RETURN： 什么都不返回
    REQUEST：   在user表中添加手机号
                curl -X POST "http://服务ip:服务端口/add_user_phone" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"phone_number":"用户名"}'
                curl -X POST "http://192.168.1.10:1900/add_user_phone" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"phone_number":"13166655511"}'
    200RETURN： 什么都不返回
    REQUEST：   在project表中添加projectid、患者名、患者年龄、患者真实姓名
                curl -X POST "http://服务ip:服务端口/add_user_phone" -H "ServiceName: service服务名" -H "Authorization: service服务token" -H "Content-Type: application/json" -d '{"project_id":"project ID", "user_name":"用户名", "user_age":用户年龄, "real_name":"用户真实姓名"}'
                curl -X POST "http://192.168.1.10:1900/create_project" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"d7aa0e59-9be6-4c76-9bf1-ee937791b8a8", "user_name":"两茫茫", "user_age":32, "real_name":"李通"}'
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

#define HTTP_OK 200
#define HTTP_FORBIDDEN 403
#define HTTP_INTERNAL 500

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
} AppConfig;

// 预处理语句句柄结构体
typedef struct {
    MYSQL *mysql;
    MYSQL_STMT *stmt_check_phone;
    MYSQL_STMT *stmt_insert_phone;
    MYSQL_STMT *stmt_add_user;
    MYSQL_STMT *stmt_get_service_passwd;
    MYSQL_STMT *stmt_insert_project;
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
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "redis_server") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerIp = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "redis_port") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPort = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(DBOP_VAR_MainConfigParse_currentKey, "redis_password") == 0) {
                    DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPassword = strdup((char*)DBOP_VAR_MainConfigParse_yamlEvent.data.scalar.value);
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
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerIp) dzlog_error("Missing configuration: redis_ip.");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPort) dzlog_error("Missing configuration: redis_port.");
    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPassword) dzlog_error("Missing configuration: redis_password.");

    if (!DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_serverPort ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_logFile ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_logConfig ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbIp ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbServerPort ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbPrivilegeUser ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbPrivilegePassword ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_dbName ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerIp ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPort ||
        !DBOP_VAR_MainConfigParse_mainConfig.DBOP_GLV_redisServerPassword) {
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
}


// ------------------------redis操作逻辑开始----------------------------


// 定义redis连接池
typedef struct {
    redisContext** DBOP_GLV_redisConnections; // 指向Redis连接的指针数组
    int DBOP_GLV_poolSize;
    pthread_mutex_t DBOP_GLV_poolMutex; // 用于同步访问的互斥锁
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
    pthread_mutex_init(&DBOP_VAR_InitializeRedisPool_pool->DBOP_GLV_poolMutex, NULL);

    for (int i = 0; i < poolSize; i++) {
        DBOP_VAR_InitializeRedisPool_pool->DBOP_GLV_redisConnections[i] = DBOP_FUN_InitializeRedis(config);
    }

    return DBOP_VAR_InitializeRedisPool_pool;
}


// 检查redis是否可用，如果不可用则重新初始化redis连接
// 正常返回true，异常且重新初始化redis连接失败则停止服务运行
bool DBOP_FUN_CheckAndReinitializeRedis(const AppConfig* DBOP_VAR_CheckAndReinitializeRedis_config, int DBOP_VAR_CheckAndReinitializeRedis_index) {
    // 使用互斥锁，让DBOP_FUN_CheckAndReinitializeRedis永远单线程执行
    // pthread_mutex_lock(&DBOP_GLV_redisConnection_mutex);
    // 使用一个简单的命令来检查Redis连接
    if (DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index] != NULL) {
        redisReply *reply = redisCommand(DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_CheckAndReinitializeRedis_index], "PING");
        if (reply != NULL && reply->type != REDIS_REPLY_ERROR) {
            freeReplyObject(reply);
            // 解除互斥锁
            pthread_mutex_unlock(&DBOP_GLV_redisConnection_mutex);
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

    // 解除互斥锁
    // pthread_mutex_unlock(&DBOP_GLV_redisConnection_mutex);
    return true;
}


// ------------------------redis操作逻辑结束----------------------------


// ------------------------鉴权逻辑开始----------------------------


bool DBOP_FUN_AuthenticateRequest(const AppConfig* DBOP_VAR_AuthenticateRequest_appConfig, const char* DBOP_VAR_AuthenticateRequest_serviceName, const char* DBOP_VAR_AuthenticateRequest_authToken) {
    bool DBOP_VAR_AuthenticateRequest_isAuthenticated = false;
    int DBOP_VAR_AuthenticateRequest_index = rand() % 10;
    if (DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_AuthenticateRequest_index] == NULL || DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_AuthenticateRequest_index]->err) {
        // 如果连接无效，调用DBOP_FUN_CheckAndReinitializeRedis来尝试恢复连接
        if (!DBOP_FUN_CheckAndReinitializeRedis(DBOP_VAR_AuthenticateRequest_appConfig, DBOP_VAR_AuthenticateRequest_index)) {
            // 如果连接仍然无法恢复，处理错误
            dzlog_error("Redis operation failed: Redis connection cannot be established.\n");
            return DBOP_VAR_AuthenticateRequest_isAuthenticated;
        }
    }
    redisReply *DBOP_VAR_AuthenticateRequest_reply = redisCommand(DBOP_GLV_redisConnectPool->DBOP_GLV_redisConnections[DBOP_VAR_AuthenticateRequest_index], "GET %s", DBOP_VAR_AuthenticateRequest_serviceName);
    if (DBOP_VAR_AuthenticateRequest_reply != NULL && DBOP_VAR_AuthenticateRequest_reply->type == REDIS_REPLY_STRING) {
        DBOP_VAR_AuthenticateRequest_isAuthenticated = (strcmp(DBOP_VAR_AuthenticateRequest_reply->str, DBOP_VAR_AuthenticateRequest_authToken) == 0);
    }
    freeReplyObject(DBOP_VAR_AuthenticateRequest_reply);
    return DBOP_VAR_AuthenticateRequest_isAuthenticated;
}


// ------------------------鉴权逻辑结束----------------------------


// ------------------------mysql通用操作逻辑开始----------------------------


// mysql声明连接池
DB_CONNECTION *DBOP_GLV_mysqlConnectPool[10];


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

    DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_phone = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *insert_phone_sql = "INSERT INTO user (user_id, phone_number) VALUES (?, ?);";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_phone, insert_phone_sql, strlen(insert_phone_sql))) {
        dzlog_error("Failed to prepare insert statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_phone));
        exit(EXIT_FAILURE);
    }

    DBOP_VAR_InitializeMySQLConnection_connect->stmt_add_user = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *add_user_sql = "INSERT INTO user (user_id, user_name, user_password) VALUES (?, ?, ?);";
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
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_project = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *insert_project_sql = "INSERT INTO project (project_id, user_name, user_age, real_name) VALUES (?, ?, ?, ?);";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_project, insert_project_sql, strlen(insert_project_sql))) {
        dzlog_error("Failed to prepare insert statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_project));
        exit(EXIT_FAILURE);
    }
}

// 初始化mysql连接池
void DBOP_FUN_InitializeConnPool(AppConfig *DBOP_VAR_InitializeConnPool_cfg) {
    for (int i = 0; i < 10; i++) {
        DBOP_GLV_mysqlConnectPool[i] = malloc(sizeof(DB_CONNECTION));  // 分配内存
        DBOP_FUN_InitializeMySQLConnection(DBOP_GLV_mysqlConnectPool[i], DBOP_VAR_InitializeConnPool_cfg);
    }
}

// 销毁mysql连接池
void DBOP_FUN_DestroyConnPool() {
    for (int i = 0; i < 10; i++) {
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_check_phone != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_check_phone);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_insert_phone != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_insert_phone);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_add_user != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_add_user);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_get_service_passwd != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_get_service_passwd);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_insert_project != NULL) {
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_insert_project);
        }
        if (DBOP_GLV_mysqlConnectPool[i]->mysql != NULL) {
            mysql_close(DBOP_GLV_mysqlConnectPool[i]->mysql);
        }
        free(DBOP_GLV_mysqlConnectPool[i]);
    }
    dzlog_info("MySQL connection pool has been destroyed.");
}


// 重新初始化mysql连接
void DBOP_FUN_ReinitializeConnPool(AppConfig *DBOP_VAR_ReinitializeConnPool_cfg, int index) {
    DB_CONNECTION *DBOP_VAR_ReinitializeConnPool_connect = DBOP_GLV_mysqlConnectPool[index];
    
    // 先关闭旧的连接和预处理语句句柄
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_check_phone != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_check_phone);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_insert_phone != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_insert_phone);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_add_user != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_add_user);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_get_service_passwd != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_get_service_passwd);
    }
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_insert_project != NULL) {
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_insert_project);
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
    DB_CONNECTION *DBOP_VAR_GetConnectFromPool_connect = DBOP_GLV_mysqlConnectPool[index];
    
    if (mysql_ping(DBOP_VAR_GetConnectFromPool_connect->mysql) != 0) {  // 检查连接是否有效
        dzlog_error("Lost connection to database with index: %d. Reconnecting...", index);
        DBOP_FUN_ReinitializeConnPool(DBOP_VAR_GetConnectFromPool_cfg, index);
    }

    return DBOP_VAR_GetConnectFromPool_connect;
}


// ------------------------mysql通用操作逻辑结束----------------------------



// ------------------------mysql添加user_phone api逻辑开始----------------------------


// 添加用户的sql数据化输出
// 样例，后续按需添加相应sql函数
int DBOP_FUN_ExecuteAddUserPhone(DB_CONNECTION *DBOP_VAR_ExecuteAddUserPhone_connect, const char *DBOP_VAR_ExecuteAddUserPhone_waitAddUserPhone, const char *DBOP_VAR_ExecuteAddUserPhone_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteAddUserPhone is checking", DBOP_VAR_ExecuteAddUserPhone_requestId);

    char DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone[256];
    strncpy(DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone, DBOP_VAR_ExecuteAddUserPhone_waitAddUserPhone, sizeof(DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone) - 1);
    DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone[sizeof(DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone) - 1] = '\0';

    // 检查phone是否存在
    MYSQL_BIND DBOP_VAR_ExecuteAddUserPhone_checkBind[1];
    memset(DBOP_VAR_ExecuteAddUserPhone_checkBind, 0, sizeof(DBOP_VAR_ExecuteAddUserPhone_checkBind));

    DBOP_VAR_ExecuteAddUserPhone_checkBind[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUserPhone_checkBind[0].buffer = (char *)DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone;
    DBOP_VAR_ExecuteAddUserPhone_checkBind[0].buffer_length = strlen(DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone, DBOP_VAR_ExecuteAddUserPhone_checkBind)) {
        dzlog_error("[req: %s] Failed to bind check param: %s", DBOP_VAR_ExecuteAddUserPhone_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone));
        return -1; // 表示绑定参数失败
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone)) {
        dzlog_error("[req: %s] Failed to execute check statement: %s", DBOP_VAR_ExecuteAddUserPhone_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone));
        return -1; // 表示执行语句失败
    }

    MYSQL_BIND DBOP_VAR_ExecuteAddUserPhone_checkResult[1];
    memset(DBOP_VAR_ExecuteAddUserPhone_checkResult, 0, sizeof(DBOP_VAR_ExecuteAddUserPhone_checkResult));
    int count;
    DBOP_VAR_ExecuteAddUserPhone_checkResult[0].buffer_type = MYSQL_TYPE_LONG;
    DBOP_VAR_ExecuteAddUserPhone_checkResult[0].buffer = &count;

    if (mysql_stmt_bind_result(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone, DBOP_VAR_ExecuteAddUserPhone_checkResult)) {
        dzlog_error("[req: %s] Failed to bind check result: %s", DBOP_VAR_ExecuteAddUserPhone_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone));
        return -1; // 表示绑定结果失败
    }

    if (mysql_stmt_fetch(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone)) {
        dzlog_error("[req: %s] Failed to fetch check result: %s", DBOP_VAR_ExecuteAddUserPhone_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone));
        return -1; // 表示获取结果失败
    }
    mysql_stmt_free_result(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_check_phone);

    if (count > 0) {
        dzlog_info("[req: %s] Phone number already exists", DBOP_VAR_ExecuteAddUserPhone_requestId);
        return 1; // 表示phone已存在
    }

    // 插入新的phone
    char DBOP_VAR_ExecuteAddUserPhone_userId[37];
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    uuid_unparse_lower(binuuid, DBOP_VAR_ExecuteAddUserPhone_userId);

    MYSQL_BIND DBOP_VAR_ExecuteAddUserPhone_bindParams[2];
    memset(DBOP_VAR_ExecuteAddUserPhone_bindParams, 0, sizeof(DBOP_VAR_ExecuteAddUserPhone_bindParams));

    DBOP_VAR_ExecuteAddUserPhone_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUserPhone_bindParams[0].buffer = DBOP_VAR_ExecuteAddUserPhone_userId;
    DBOP_VAR_ExecuteAddUserPhone_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteAddUserPhone_userId);

    DBOP_VAR_ExecuteAddUserPhone_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUserPhone_bindParams[1].buffer = DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone;
    DBOP_VAR_ExecuteAddUserPhone_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteAddUserPhone_noConstWaitAddUserPhone);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_insert_phone, DBOP_VAR_ExecuteAddUserPhone_bindParams)) {
        dzlog_error("[req: %s] Failed to bind insert param: %s", DBOP_VAR_ExecuteAddUserPhone_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_insert_phone));
        return -1; // 表示绑定插入参数失败
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_insert_phone)) {
        dzlog_error("[req: %s] Failed to execute insert statement: %s", DBOP_VAR_ExecuteAddUserPhone_requestId, mysql_stmt_error(DBOP_VAR_ExecuteAddUserPhone_connect->stmt_insert_phone));
        return -1; // 表示执行插入语句失败
    }

    dzlog_info("[req: %s] Successfully added user phone", DBOP_VAR_ExecuteAddUserPhone_requestId);
    return 0; // 表示成功
}





// 添加用户手机的虚拟路径执行函数
void DBOP_FUN_ApiAddUserPhone(struct evhttp_request *DBOP_VAR_ApiAddUserPhone_request, void *DBOP_VAR_ApiAddUserPhone_voidCfg) {
    AppConfig *DBOP_VAR_ApiAddUserPhone_cfg = (AppConfig *)DBOP_VAR_ApiAddUserPhone_voidCfg;
    const char *DBOP_VAR_ApiAddUserPhone_requestId = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiAddUserPhone_request), "X-Request-ID");
    char uuid_str[37];  // UUID字符串的长度
    if (!DBOP_VAR_ApiAddUserPhone_requestId) {
        // 如果请求中没有X-Request-ID头部，生成一个UUID作为请求ID
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, uuid_str);
        DBOP_VAR_ApiAddUserPhone_requestId = uuid_str;
    }
    dzlog_info("[req: %s] Processing API request to ApiAddUserPhone.", DBOP_VAR_ApiAddUserPhone_requestId);
    // 解析 HTTP 请求中的查询字符串并存储在 DBOP_VAR_ApiAddUserPhone_headers 结构体中
    struct evkeyvalq DBOP_VAR_ApiAddUserPhone_headers;
    evhttp_parse_query_str(evhttp_uri_get_query(evhttp_request_get_evhttp_uri(DBOP_VAR_ApiAddUserPhone_request)), &DBOP_VAR_ApiAddUserPhone_headers);
    dzlog_info("[req: %s] Parsing query string from request.", DBOP_VAR_ApiAddUserPhone_requestId);
    // 请求鉴权
    const char *DBOP_VAR_ApiAddUserPhone_serviceName = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiAddUserPhone_request), "ServiceName");
    const char *DBOP_VAR_ApiAddUserPhone_token = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiAddUserPhone_request), "Authorization");
    if (DBOP_FUN_AuthenticateRequest(DBOP_VAR_ApiAddUserPhone_cfg, DBOP_VAR_ApiAddUserPhone_serviceName, DBOP_VAR_ApiAddUserPhone_token)) {
        dzlog_info("[req: %s] Request authentication approval.", DBOP_VAR_ApiAddUserPhone_requestId);
    } else {
        dzlog_warn("[req: %s] Unauthorized access attempt.", DBOP_VAR_ApiAddUserPhone_requestId);
        evhttp_send_reply(DBOP_VAR_ApiAddUserPhone_request, 401, "Unauthorized", NULL);
        return;
    }
    // 从 POST 数据中读取 JSON 参数
    struct evbuffer *DBOP_VAR_ApiAddUserPhone_inputBuffer = evhttp_request_get_input_buffer(DBOP_VAR_ApiAddUserPhone_request);
    size_t DBOP_VAR_ApiAddUserPhone_bufferLen = evbuffer_get_length(DBOP_VAR_ApiAddUserPhone_inputBuffer);
    char DBOP_VAR_ApiAddUserPhone_postData[DBOP_VAR_ApiAddUserPhone_bufferLen + 1];
    evbuffer_remove(DBOP_VAR_ApiAddUserPhone_inputBuffer, DBOP_VAR_ApiAddUserPhone_postData, DBOP_VAR_ApiAddUserPhone_bufferLen);
    DBOP_VAR_ApiAddUserPhone_postData[DBOP_VAR_ApiAddUserPhone_bufferLen] = '\0';

    json_error_t DBOP_VAR_ApiAddUserPhone_dataJsonError;
    json_t *DBOP_VAR_ApiAddUserPhone_dataJsonAll = json_loads(DBOP_VAR_ApiAddUserPhone_postData, 0, &DBOP_VAR_ApiAddUserPhone_dataJsonError);
    dzlog_info("Parsing POST data, request id: %s", DBOP_VAR_ApiAddUserPhone_requestId);
    if (!DBOP_VAR_ApiAddUserPhone_dataJsonAll) {
        dzlog_error("Failed to parse JSON from request body, request id: %s", DBOP_VAR_ApiAddUserPhone_requestId);
        evhttp_send_reply(DBOP_VAR_ApiAddUserPhone_request, 400, "Bad Request", NULL);
        return;
    }

    json_t *DBOP_VAR_ApiAddUserPhone_dataJsonPhoneNumber = json_object_get(DBOP_VAR_ApiAddUserPhone_dataJsonAll, "phone_number");

    if (!json_is_string(DBOP_VAR_ApiAddUserPhone_dataJsonPhoneNumber)) {
        dzlog_error("Invalid JSON data received. Expecting string types for 'phone_number'.");
        evhttp_send_reply(DBOP_VAR_ApiAddUserPhone_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiAddUserPhone_dataJsonAll);
        return;
    }
    dzlog_info("[req: %s] Validating JSON data.", DBOP_VAR_ApiAddUserPhone_requestId);

    const char *DBOP_VAR_ApiAddUserPhone_waitAddUserPhone = json_string_value(DBOP_VAR_ApiAddUserPhone_dataJsonPhoneNumber);

    dzlog_info("[req: %s] Executing database operation for ApiAddUserPhone: %s", DBOP_VAR_ApiAddUserPhone_requestId, DBOP_VAR_ApiAddUserPhone_waitAddUserPhone);
    // 调用数据库函数
    int index = rand() % 10;  // 随机选择一个索引
    DB_CONNECTION *DBOP_VAR_ApiAddUserPhone_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiAddUserPhone_cfg, index);  // 从连接池中取出一个连接

    // 发送HTTP响应
    int result = DBOP_FUN_ExecuteAddUserPhone(DBOP_VAR_ApiAddUserPhone_mysqlConnect, DBOP_VAR_ApiAddUserPhone_waitAddUserPhone, DBOP_VAR_ApiAddUserPhone_requestId);

    if (result == 1) {
        // phone 已存在，返回 403
        evhttp_send_reply(DBOP_VAR_ApiAddUserPhone_request, HTTP_FORBIDDEN, "Phone already exists", NULL);
        dzlog_info("[req: %s] Phone already exists, returning 403", DBOP_VAR_ApiAddUserPhone_requestId);
    } else if (result == 0) {
        // 成功添加，返回 200
        evhttp_send_reply(DBOP_VAR_ApiAddUserPhone_request, HTTP_OK, "OK", NULL);
        dzlog_info("[req: %s] Successfully added phone, returning 200", DBOP_VAR_ApiAddUserPhone_requestId);
    } else {
        // 其他错误，返回 500
        evhttp_send_reply(DBOP_VAR_ApiAddUserPhone_request, HTTP_INTERNAL, "Internal Server Error", NULL);
        dzlog_error("[req: %s] Failed to add phone, returning 500", DBOP_VAR_ApiAddUserPhone_requestId);
    }
}


// ------------------------mysql添加user_phone api逻辑结束----------------------------



// ------------------------mysql添加用户api逻辑开始----------------------------


// 添加用户的sql数据化输出
// 样例，后续按需添加相应sql函数
void DBOP_FUN_ExecuteAddUser(DB_CONNECTION *DBOP_VAR_ExecuteAddUser_mysqlConnect, const char *DBOP_VAR_ExecuteAddUser_waitAddUser, const char *DBOP_VAR_ExecuteAddUser_waitAddPassword) {
    dzlog_info("DBOP_FUN_ExecuteAddUser is doing, user id is: %s", DBOP_VAR_ExecuteAddUser_waitAddUser);
    //由于传入参数为只读，所以在不破坏源参数只读性的情况下，复制一个副本来进行下面的操作
    char DBOP_VAR_ExecuteAddUser_notConstAddUser[256];
    char DBOP_VAR_ExecuteAddUser_notConstAddPassword[256];
    strncpy(DBOP_VAR_ExecuteAddUser_notConstAddUser, DBOP_VAR_ExecuteAddUser_waitAddUser, sizeof(DBOP_VAR_ExecuteAddUser_notConstAddUser) - 1);
    DBOP_VAR_ExecuteAddUser_notConstAddUser[sizeof(DBOP_VAR_ExecuteAddUser_notConstAddUser) - 1] = '\0';
    strncpy(DBOP_VAR_ExecuteAddUser_notConstAddPassword, DBOP_VAR_ExecuteAddUser_waitAddPassword, sizeof(DBOP_VAR_ExecuteAddUser_notConstAddPassword) - 1);
    DBOP_VAR_ExecuteAddUser_notConstAddPassword[sizeof(DBOP_VAR_ExecuteAddUser_notConstAddPassword) - 1] = '\0';

    char DBOP_VAR_ExecuteAddUser_userId[37]; // UUID字符串长度为36个字符加上一个终止符
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    uuid_unparse_lower(binuuid, DBOP_VAR_ExecuteAddUser_userId);

    MYSQL_BIND DBOP_VAR_ExecuteAddUser_bindParams[3];
    memset(DBOP_VAR_ExecuteAddUser_bindParams, 0, sizeof(DBOP_VAR_ExecuteAddUser_bindParams));

    DBOP_VAR_ExecuteAddUser_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUser_bindParams[0].buffer = DBOP_VAR_ExecuteAddUser_userId;
    DBOP_VAR_ExecuteAddUser_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteAddUser_userId);

    DBOP_VAR_ExecuteAddUser_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUser_bindParams[1].buffer = DBOP_VAR_ExecuteAddUser_notConstAddUser;
    DBOP_VAR_ExecuteAddUser_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteAddUser_notConstAddUser);

    DBOP_VAR_ExecuteAddUser_bindParams[2].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteAddUser_bindParams[2].buffer = DBOP_VAR_ExecuteAddUser_notConstAddPassword;
    DBOP_VAR_ExecuteAddUser_bindParams[2].buffer_length = strlen(DBOP_VAR_ExecuteAddUser_notConstAddPassword);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteAddUser_mysqlConnect->stmt_add_user, DBOP_VAR_ExecuteAddUser_bindParams)) {
        dzlog_error("Failed to DBOP_VAR_ExecuteAddUser_bindParams bind param: %s", mysql_stmt_error(DBOP_VAR_ExecuteAddUser_mysqlConnect->stmt_add_user));
        return;
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteAddUser_mysqlConnect->stmt_add_user)) {
        dzlog_error("Failed to execute DBOP_FUN_ExecuteAddUser: %s", mysql_stmt_error(DBOP_VAR_ExecuteAddUser_mysqlConnect->stmt_add_user));
        return;
    }
}


// 添加用户的虚拟路径执行函数
void DBOP_FUN_ApiAddUser(struct evhttp_request *DBOP_VAR_ApiAddUser_request, void *DBOP_VAR_ApiAddUser_voidCfg) {
    AppConfig *DBOP_VAR_ApiAddUser_cfg = (AppConfig *)DBOP_VAR_ApiAddUser_voidCfg;
    const char *DBOP_VAR_ApiAddUser_requestId = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiAddUser_request), "X-Request-ID");
    char uuid_str[37];  // UUID字符串的长度
    if (!DBOP_VAR_ApiAddUser_requestId) {
        // 如果请求中没有X-Request-ID头部，生成一个UUID作为请求ID
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, uuid_str);
        DBOP_VAR_ApiAddUser_requestId = uuid_str;
    }
    dzlog_info("Processing API request to ApiAddUser, request id: %s", DBOP_VAR_ApiAddUser_requestId);
    // 解析 HTTP 请求中的查询字符串并存储在 DBOP_VAR_ApiAddUser_headers 结构体中
    struct evkeyvalq DBOP_VAR_ApiAddUser_headers;
    evhttp_parse_query_str(evhttp_uri_get_query(evhttp_request_get_evhttp_uri(DBOP_VAR_ApiAddUser_request)), &DBOP_VAR_ApiAddUser_headers);
    dzlog_info("Parsing query string from request, request id: %s", DBOP_VAR_ApiAddUser_requestId);
    // 请求鉴权
    const char *DBOP_VAR_ApiAddUser_serviceName = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiAddUser_request), "ServiceName");
    const char *DBOP_VAR_ApiAddUser_token = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiAddUser_request), "Authorization");
    if (DBOP_FUN_AuthenticateRequest(DBOP_VAR_ApiAddUser_cfg, DBOP_VAR_ApiAddUser_serviceName, DBOP_VAR_ApiAddUser_token)) {
        dzlog_info("Request authentication approval, request id: %s .", DBOP_VAR_ApiAddUser_requestId);
    } else {
        dzlog_warn("Unauthorized access attempt, request id: %s", DBOP_VAR_ApiAddUser_requestId);
        evhttp_send_reply(DBOP_VAR_ApiAddUser_request, 401, "Unauthorized", NULL);
        return;
    }
    // 从 POST 数据中读取 JSON 参数
    struct evbuffer *DBOP_VAR_ApiAddUser_inputBuffer = evhttp_request_get_input_buffer(DBOP_VAR_ApiAddUser_request);
    size_t DBOP_VAR_ApiAddUser_bufferLen = evbuffer_get_length(DBOP_VAR_ApiAddUser_inputBuffer);
    char DBOP_VAR_ApiAddUser_postData[DBOP_VAR_ApiAddUser_bufferLen + 1];
    evbuffer_remove(DBOP_VAR_ApiAddUser_inputBuffer, DBOP_VAR_ApiAddUser_postData, DBOP_VAR_ApiAddUser_bufferLen);
    DBOP_VAR_ApiAddUser_postData[DBOP_VAR_ApiAddUser_bufferLen] = '\0';

    json_error_t DBOP_VAR_ApiAddUser_dataJsonError;
    json_t *DBOP_VAR_ApiAddUser_dataJsonAll = json_loads(DBOP_VAR_ApiAddUser_postData, 0, &DBOP_VAR_ApiAddUser_dataJsonError);
    dzlog_info("Parsing POST data, request id: %s", DBOP_VAR_ApiAddUser_requestId);
    if (!DBOP_VAR_ApiAddUser_dataJsonAll) {
        dzlog_error("Failed to parse JSON from request body, request id: %s", DBOP_VAR_ApiAddUser_requestId);
        evhttp_send_reply(DBOP_VAR_ApiAddUser_request, 400, "Bad Request", NULL);
        return;
    }

    json_t *DBOP_VAR_ApiAddUser_dataJsonUser = json_object_get(DBOP_VAR_ApiAddUser_dataJsonAll, "user");
    json_t *DBOP_VAR_ApiAddUser_dataJsonPassword = json_object_get(DBOP_VAR_ApiAddUser_dataJsonAll, "password");

    if (!json_is_string(DBOP_VAR_ApiAddUser_dataJsonUser) || !json_is_string(DBOP_VAR_ApiAddUser_dataJsonPassword)) {
        dzlog_error("Invalid JSON data received. Expecting string types for 'user' and 'password'.");
        evhttp_send_reply(DBOP_VAR_ApiAddUser_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiAddUser_dataJsonAll);
        return;
    }
    dzlog_info("Validating JSON data, request id: %s", DBOP_VAR_ApiAddUser_requestId);

    const char *DBOP_VAR_ApiAddUser_waitAddUser = json_string_value(DBOP_VAR_ApiAddUser_dataJsonUser);
    const char *DBOP_VAR_ApiAddUser_waitAddPassword = json_string_value(DBOP_VAR_ApiAddUser_dataJsonPassword);

    dzlog_info("[%s] Executing database operation for ApiAddUser: %s", DBOP_VAR_ApiAddUser_requestId, DBOP_VAR_ApiAddUser_waitAddUser);
    // 调用数据库函数
    int index = rand() % 10;  // 随机选择一个索引
    DB_CONNECTION *DBOP_VAR_ApiAddUser_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiAddUser_cfg, index);  // 从连接池中取出一个连接
    DBOP_FUN_ExecuteAddUser(DBOP_VAR_ApiAddUser_mysqlConnect, DBOP_VAR_ApiAddUser_waitAddUser, DBOP_VAR_ApiAddUser_waitAddPassword);

    // 发送HTTP响应
    evhttp_send_reply(DBOP_VAR_ApiAddUser_request, HTTP_OK, "OK", NULL);
    dzlog_info("Sending HTTP response, request id: %s", DBOP_VAR_ApiAddUser_requestId);
}


// ------------------------mysql添加用户api逻辑结束----------------------------


// ------------------------mysql创建project api逻辑开始----------------------------

// 创建项目的sql数据化输出
int DBOP_FUN_ExecuteCreateProject(DB_CONNECTION *DBOP_VAR_ExecuteCreateProject_connect, const char *DBOP_VAR_ExecuteCreateProject_projectId, const char *DBOP_VAR_ExecuteCreateProject_userName, int DBOP_VAR_ExecuteCreateProject_userAge, const char *DBOP_VAR_ExecuteCreateProject_realName, const char *DBOP_VAR_ExecuteCreateProject_requestId) {
    dzlog_info("[req: %s] DBOP_FUN_ExecuteCreateProject is starting", DBOP_VAR_ExecuteCreateProject_requestId);

    MYSQL_BIND DBOP_VAR_ExecuteCreateProject_bindParams[4];
    memset(DBOP_VAR_ExecuteCreateProject_bindParams, 0, sizeof(DBOP_VAR_ExecuteCreateProject_bindParams));

    // 绑定project_id
    DBOP_VAR_ExecuteCreateProject_bindParams[0].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindParams[0].buffer = (char *)DBOP_VAR_ExecuteCreateProject_projectId;
    DBOP_VAR_ExecuteCreateProject_bindParams[0].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_projectId);

    // 绑定user_name
    DBOP_VAR_ExecuteCreateProject_bindParams[1].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindParams[1].buffer = (char *)DBOP_VAR_ExecuteCreateProject_userName;
    DBOP_VAR_ExecuteCreateProject_bindParams[1].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_userName);

    // 绑定user_age
    DBOP_VAR_ExecuteCreateProject_bindParams[2].buffer_type = MYSQL_TYPE_TINY;
    DBOP_VAR_ExecuteCreateProject_bindParams[2].buffer = &DBOP_VAR_ExecuteCreateProject_userAge;
    DBOP_VAR_ExecuteCreateProject_bindParams[2].buffer_length = sizeof(DBOP_VAR_ExecuteCreateProject_userAge);

    // 绑定real_name
    DBOP_VAR_ExecuteCreateProject_bindParams[3].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreateProject_bindParams[3].buffer = (char *)DBOP_VAR_ExecuteCreateProject_realName;
    DBOP_VAR_ExecuteCreateProject_bindParams[3].buffer_length = strlen(DBOP_VAR_ExecuteCreateProject_realName);

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreateProject_connect->stmt_insert_project, DBOP_VAR_ExecuteCreateProject_bindParams)) {
        dzlog_error("[req: %s] Failed to bind insert param: %s", DBOP_VAR_ExecuteCreateProject_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateProject_connect->stmt_insert_project));
        return -1; // 表示绑定插入参数失败
    }

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreateProject_connect->stmt_insert_project)) {
        dzlog_error("[req: %s] Failed to execute insert statement: %s", DBOP_VAR_ExecuteCreateProject_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreateProject_connect->stmt_insert_project));
        return -1; // 表示执行插入语句失败
    }

    dzlog_info("[req: %s] Successfully created project", DBOP_VAR_ExecuteCreateProject_requestId);
    return 0; // 表示成功
}

// 创建项目的API接口
void DBOP_FUN_ApiCreateProject(struct evhttp_request *DBOP_VAR_ApiCreateProject_request, void *DBOP_VAR_ApiCreateProject_voidCfg) {
    AppConfig *DBOP_VAR_ApiCreateProject_cfg = (AppConfig *)DBOP_VAR_ApiCreateProject_voidCfg;
    const char *DBOP_VAR_ApiCreateProject_requestId = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiCreateProject_request), "X-Request-ID");
    char uuid_str[37];  // UUID字符串的长度
    if (!DBOP_VAR_ApiCreateProject_requestId) {
        // 如果请求中没有X-Request-ID头部，生成一个UUID作为请求ID
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, uuid_str);
        DBOP_VAR_ApiCreateProject_requestId = uuid_str;
    }
    dzlog_info("[req: %s] Processing API request to ApiCreateProject.", DBOP_VAR_ApiCreateProject_requestId);

    // 解析 HTTP 请求中的查询字符串并存储在 DBOP_VAR_ApiCreateProject_headers 结构体中
    struct evkeyvalq DBOP_VAR_ApiCreateProject_headers;
    evhttp_parse_query_str(evhttp_uri_get_query(evhttp_request_get_evhttp_uri(DBOP_VAR_ApiCreateProject_request)), &DBOP_VAR_ApiCreateProject_headers);

    // 请求鉴权
    const char *DBOP_VAR_ApiCreateProject_serviceName = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiCreateProject_request), "ServiceName");
    const char *DBOP_VAR_ApiCreateProject_token = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiCreateProject_request), "Authorization");
    if (DBOP_FUN_AuthenticateRequest(DBOP_VAR_ApiCreateProject_cfg, DBOP_VAR_ApiCreateProject_serviceName, DBOP_VAR_ApiCreateProject_token)) {
        dzlog_info("[req: %s] Request authentication approval.", DBOP_VAR_ApiCreateProject_requestId);
    } else {
        dzlog_warn("[req: %s] Unauthorized access attempt.", DBOP_VAR_ApiCreateProject_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, 401, "Unauthorized", NULL);
        return;
    }

    // 从 POST 数据中读取 JSON 参数
    struct evbuffer *DBOP_VAR_ApiCreateProject_inputBuffer = evhttp_request_get_input_buffer(DBOP_VAR_ApiCreateProject_request);
    size_t DBOP_VAR_ApiCreateProject_bufferLen = evbuffer_get_length(DBOP_VAR_ApiCreateProject_inputBuffer);
    char DBOP_VAR_ApiCreateProject_postData[DBOP_VAR_ApiCreateProject_bufferLen + 1];
    evbuffer_remove(DBOP_VAR_ApiCreateProject_inputBuffer, DBOP_VAR_ApiCreateProject_postData, DBOP_VAR_ApiCreateProject_bufferLen);
    DBOP_VAR_ApiCreateProject_postData[DBOP_VAR_ApiCreateProject_bufferLen] = '\0';

    json_error_t DBOP_VAR_ApiCreateProject_dataJsonError;
    json_t *DBOP_VAR_ApiCreateProject_dataJsonAll = json_loads(DBOP_VAR_ApiCreateProject_postData, 0, &DBOP_VAR_ApiCreateProject_dataJsonError);
    dzlog_info("[req: %s] Parsing POST data.", DBOP_VAR_ApiCreateProject_requestId);
    if (!DBOP_VAR_ApiCreateProject_dataJsonAll) {
        dzlog_error("[req: %s] Failed to parse JSON from request body.", DBOP_VAR_ApiCreateProject_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, 400, "Bad Request", NULL);
        return;
    }

    json_t *DBOP_VAR_ApiCreateProject_dataJsonProjectId = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "project_id");
    json_t *DBOP_VAR_ApiCreateProject_dataJsonUserName = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "user_name");
    json_t *DBOP_VAR_ApiCreateProject_dataJsonUserAge = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "user_age");
    json_t *DBOP_VAR_ApiCreateProject_dataJsonRealName = json_object_get(DBOP_VAR_ApiCreateProject_dataJsonAll, "real_name");

    if (!json_is_string(DBOP_VAR_ApiCreateProject_dataJsonProjectId) || 
        !json_is_string(DBOP_VAR_ApiCreateProject_dataJsonUserName) ||
        !json_is_integer(DBOP_VAR_ApiCreateProject_dataJsonUserAge) || 
        !json_is_string(DBOP_VAR_ApiCreateProject_dataJsonRealName)) {
        dzlog_error("[req: %s] Invalid JSON data received.", DBOP_VAR_ApiCreateProject_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiCreateProject_dataJsonAll);
        return;
    }

    dzlog_info("[req: %s] Executing database operation for ApiCreateProject.", DBOP_VAR_ApiCreateProject_requestId);

    const char *DBOP_VAR_ApiCreateProject_projectId = json_string_value(DBOP_VAR_ApiCreateProject_dataJsonProjectId);
    const char *DBOP_VAR_ApiCreateProject_userName = json_string_value(DBOP_VAR_ApiCreateProject_dataJsonUserName);
    int DBOP_VAR_ApiCreateProject_userAge = json_integer_value(DBOP_VAR_ApiCreateProject_dataJsonUserAge);
    const char *DBOP_VAR_ApiCreateProject_realName = json_string_value(DBOP_VAR_ApiCreateProject_dataJsonRealName);

    // 调用数据库函数
    int index = rand() % 10;  // 随机选择一个索引
    DB_CONNECTION *DBOP_VAR_ApiCreateProject_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiCreateProject_cfg, index);  // 从连接池中取出一个连接

    // 发送HTTP响应
    int result = DBOP_FUN_ExecuteCreateProject(DBOP_VAR_ApiCreateProject_mysqlConnect, DBOP_VAR_ApiCreateProject_projectId, DBOP_VAR_ApiCreateProject_userName, DBOP_VAR_ApiCreateProject_userAge, DBOP_VAR_ApiCreateProject_realName, DBOP_VAR_ApiCreateProject_requestId);

    if (result == 0) {
        // 成功创建，返回 200
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, HTTP_OK, "OK", NULL);
        dzlog_info("[req: %s] Successfully created project, returning 200", DBOP_VAR_ApiCreateProject_requestId);
    } else {
        // 其他错误，返回 500
        evhttp_send_reply(DBOP_VAR_ApiCreateProject_request, HTTP_INTERNAL, "Internal Server Error", NULL);
        dzlog_error("[req: %s] Failed to create project, returning 500", DBOP_VAR_ApiCreateProject_requestId);
    }

    json_decref(DBOP_VAR_ApiCreateProject_dataJsonAll);
}

// ------------------------mysql创建project api逻辑结束----------------------------


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
    // 调用数据库函数
    int index = rand() % 10;  // 随机选择一个索引
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


int main() { 
    AppConfig DBOP_VAR_Main_cfg = DBOP_FUN_MainConfigParse("config/config.yaml"); //初始化结构体
    struct event_base *DBOP_VAR_Main_eventBase = event_base_new();
    struct evhttp *DBOP_VAR_Main_httpServer = evhttp_new(DBOP_VAR_Main_eventBase);
    srand(time(NULL));

    DBOP_GLV_redisConnectPool = DBOP_FUN_InitializeRedisPool(&DBOP_VAR_Main_cfg, 10);
    
    DBOP_FUN_InitLogging(&DBOP_VAR_Main_cfg);
    
    DBOP_FUN_InitializeConnPool(&DBOP_VAR_Main_cfg);

    // 路径路由
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/add_user_from_pool", DBOP_FUN_ApiAddUser, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/get_service_passwd", DBOP_FUN_ApiGetServicePasswd, &DBOP_VAR_Main_cfg);
    //evhttp_set_cb(DBOP_VAR_Main_httpServer, "/get_user_by_phone", DBOP_FUN_ApiGetUserByPhone, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/add_user_phone", DBOP_FUN_ApiAddUserPhone, &DBOP_VAR_Main_cfg);
    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/create_project", DBOP_FUN_ApiCreateProject, &DBOP_VAR_Main_cfg);

    // 绑定到 0.0.0.0:DBOP_GLV_serverPort
    if (evhttp_bind_socket(DBOP_VAR_Main_httpServer, "0.0.0.0", atoi(DBOP_VAR_Main_cfg.DBOP_GLV_serverPort)) != 0) {
        dzlog_error("Failed to bind to port %s", DBOP_VAR_Main_cfg.DBOP_GLV_serverPort);
        return 1;
    }

    // 启动事件循环
    event_base_dispatch(DBOP_VAR_Main_eventBase);

    // 释放 libevent 和 libevhttp 资源
    evhttp_free(DBOP_VAR_Main_httpServer);
    event_base_free(DBOP_VAR_Main_eventBase);

    // 销毁数据库连接池和释放配置资源
    DBOP_FUN_DestroyConnPool();
    DBOP_FUN_FreeConfig(&DBOP_VAR_Main_cfg);
    //关闭zlog
    zlog_fini();
    return 0;
}
