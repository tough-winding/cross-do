// 整体目的：用以给服务间通讯的各个组件生成请求用token，对内发布socket
// 编译命令： gcc -o identity_access_management.exe identity_access_management.c -lyaml -lhiredis -lcurl -levent -levent_pthreads -lssl -lcrypto -lpthread -lzlog -luuid -ljansson -std=c99 -g -O3
/*
    访问接口样例及返回体样例
    REQUEST:    根据加密串和service服务名，时间戳，来获取30分钟时限的token
                curl -X POST "http://服务IP:服务域名/get_token" -H "Content-Type: application/json" -d '{"service_username":"要获取token的服务名", "encrypt_str":"加密计算后的字符串", "date": "时间戳"}'
                curl -X POST "http://192.168.1.10:10089/get_token" -H "Content-Type: application/json" -d '{"service_username":"ftp", "encrypt_str":"68a165f5db68a8202e6e334f216ad9f74309dfebfb4395f2fd776af87b98b493", "date": "2024-04-16 17:20:10"}'
    200RETURN:  {"Token":"sdkRQIWSshA99J5gYE69Vl3sCV4K66by"}
*/
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <yaml.h>
#include <hiredis/hiredis.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <pthread.h>
#include <zlog.h>
#include <uuid/uuid.h>
#include <jansson.h>

// libvent多线程支持头文件
#include <event2/thread.h>

// 目前为单线程、http
// 代码结构支持多线程场景，必要时只需要解除多线程支持的注释即可
// 未来要改为https
// 目前错误日志为标准错误输出来测试，后续要用log4c
// 大部分注释为通过ChatGPT4添加，不保证准确性，仅供参考，后续会手工写下本文件注释，并由ChatGPT4生成一个逐行注释版，类似database的那个注释版用于各位参考。
/*
变量命名要求如下：
    函数        模块名缩写(全大写)_FUN_函数名(大驼峰命名)
    常量        模块名缩写(全大写)_CON_常量名(小驼峰命名)
    全局变量    模块名缩写(全大写)_GLV_变量名(小驼峰命名)
    变量        模块名缩写(全大写)_VAR_函数名(大驼峰命名)_变量名(小驼峰命名)
*/


#define IAM_CON_tokenLength 32 
char* IAM_GLV_selfUseToken = NULL; 
redisContext* IAM_GLV_redisConnection = NULL;
pthread_mutex_t IAM_GLV_redisConnection_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char* IAM_GLV_redisServerIp;
    char* IAM_GLV_redisServerPort;
    char* IAM_GLV_redisServerPassword;
    char* IAM_GLV_dbopServiceHost;
    char* IAM_GLV_dbopServicePort;
    char* IAM_GLV_serverIp;
    char* IAM_GLV_serverPort;
    char* IAM_GLV_logConfig;
} AppConfig;

// 初始化dzlog
void IAM_FUN_InitLogging(AppConfig *IAM_VAR_InitLogging_cfg) {
    int rc;
    rc = dzlog_init(IAM_VAR_InitLogging_cfg->IAM_GLV_logConfig, "whatever");
    if (rc) {
        fprintf(stderr, "zlog init failed\n");
        exit(EXIT_FAILURE);
    }
}

struct event *IAM_GLV_tokenRefreshEvent;


// 解析配置文件
// 正确返回为config字典，错误返回为NULL，或终止服务
AppConfig IAM_FUN_MainConfigParse(const char* filename) {
    FILE *IAM_VAR_MainConfigParse_fileHandle = fopen(filename, "r");
    yaml_parser_t IAM_VAR_MainConfigParse_yamlParser;
    yaml_event_t IAM_VAR_MainConfigParse_yamlEvent;

    if (!yaml_parser_initialize(&IAM_VAR_MainConfigParse_yamlParser)) {
        fprintf(stderr, "Failed to initialize IAM_VAR_MainConfigParse_yamlParser\n");
        exit(EXIT_FAILURE);
    }
    if (IAM_VAR_MainConfigParse_fileHandle == NULL) {
        fprintf(stderr, "Failed to open file\n");
        exit(EXIT_FAILURE);
    }
    yaml_parser_set_input_file(&IAM_VAR_MainConfigParse_yamlParser, IAM_VAR_MainConfigParse_fileHandle);

    AppConfig IAM_VAR_MainConfigParse_mainConfig = {0};
    char* IAM_VAR_MainConfigParse_currentKey = NULL;

    // 解析YAML文件
    bool done = false;
    while (!done) {
        if (!yaml_parser_parse(&IAM_VAR_MainConfigParse_yamlParser, &IAM_VAR_MainConfigParse_yamlEvent)) {
            fprintf(stderr, "Parser error\n");
            exit(EXIT_FAILURE);
        }

        switch(IAM_VAR_MainConfigParse_yamlEvent.type) {
        case YAML_SCALAR_EVENT:
            if (IAM_VAR_MainConfigParse_currentKey) {
                if (strcmp(IAM_VAR_MainConfigParse_currentKey, "redis_server") == 0) {
                    IAM_VAR_MainConfigParse_mainConfig.IAM_GLV_redisServerIp = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(IAM_VAR_MainConfigParse_currentKey, "redis_port") == 0) {
                    IAM_VAR_MainConfigParse_mainConfig.IAM_GLV_redisServerPort = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(IAM_VAR_MainConfigParse_currentKey, "redis_password") == 0) {
                    IAM_VAR_MainConfigParse_mainConfig.IAM_GLV_redisServerPassword = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(IAM_VAR_MainConfigParse_currentKey, "dbop_service_url") == 0) {
                    IAM_VAR_MainConfigParse_mainConfig.IAM_GLV_dbopServiceHost = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(IAM_VAR_MainConfigParse_currentKey, "dbop_service_port") == 0) {
                    IAM_VAR_MainConfigParse_mainConfig.IAM_GLV_dbopServicePort = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(IAM_VAR_MainConfigParse_currentKey, "server_ip") == 0) {
                    IAM_VAR_MainConfigParse_mainConfig.IAM_GLV_serverIp = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(IAM_VAR_MainConfigParse_currentKey, "server_port") == 0) {
                    IAM_VAR_MainConfigParse_mainConfig.IAM_GLV_serverPort = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                } else if (strcmp(IAM_VAR_MainConfigParse_currentKey, "log_config") == 0) {
                    IAM_VAR_MainConfigParse_mainConfig.IAM_GLV_logConfig = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
                }
                free(IAM_VAR_MainConfigParse_currentKey);
                IAM_VAR_MainConfigParse_currentKey = NULL;
            } else {
                IAM_VAR_MainConfigParse_currentKey = strdup((char*)IAM_VAR_MainConfigParse_yamlEvent.data.scalar.value);
            }
            break;
        case YAML_STREAM_END_EVENT:
            done = true;
            break;
        default:
            break;
        }

        yaml_event_delete(&IAM_VAR_MainConfigParse_yamlEvent);
    }

    yaml_parser_delete(&IAM_VAR_MainConfigParse_yamlParser);
    fclose(IAM_VAR_MainConfigParse_fileHandle);

    return IAM_VAR_MainConfigParse_mainConfig;
}


// 定义redis连接池
typedef struct {
    redisContext** IAM_GLV_redisConnections; // 指向Redis连接的指针数组
    int IAM_GLV_poolSize;
    pthread_mutex_t IAM_GLV_poolMutex; // 用于同步访问的互斥锁
} RedisPool;

RedisPool* IAM_GLV_redisConnectPool = NULL;

// 初始化和连接到Redis
// 正确返回为redis_connect的连接，错误返回为NULL
redisContext* IAM_FUN_InitializeRedis(const AppConfig* IAM_VAR_InitializeRedis_config) {
    char* endptr;
    long IAM_VAR_InitializeRedis_redisPort = strtol(IAM_VAR_InitializeRedis_config->IAM_GLV_redisServerPort, &endptr, 10);
    if (*endptr != '\0') {
        dzlog_error("Invalid port number: Non-numeric characters present.");
        return NULL;
    }
    redisContext *IAM_VAR_InitializeRedis_redisConnect = redisConnect(IAM_VAR_InitializeRedis_config->IAM_GLV_redisServerIp, (int)IAM_VAR_InitializeRedis_redisPort);
    if (IAM_VAR_InitializeRedis_redisConnect == NULL || IAM_VAR_InitializeRedis_redisConnect->err) {
        if (IAM_VAR_InitializeRedis_redisConnect) {
            dzlog_error("Redis connection error: %s.", IAM_VAR_InitializeRedis_redisConnect->errstr);
            redisFree(IAM_VAR_InitializeRedis_redisConnect);
        } else {
            dzlog_error("Connection error: can't allocate redis context.");
        }
        return NULL;
    }

    // 如果提供了密码，则使用它进行认证
    if (IAM_VAR_InitializeRedis_config->IAM_GLV_redisServerPassword != NULL && strlen(IAM_VAR_InitializeRedis_config->IAM_GLV_redisServerPassword) > 0) {
        redisReply *reply = redisCommand(IAM_VAR_InitializeRedis_redisConnect, "AUTH %s", IAM_VAR_InitializeRedis_config->IAM_GLV_redisServerPassword);
        if (reply->type == REDIS_REPLY_ERROR) {
            dzlog_error("Auth error: %s.", reply->str);
            freeReplyObject(reply);
            redisFree(IAM_VAR_InitializeRedis_redisConnect);
            return NULL;
        }
        freeReplyObject(reply);
    }

    return IAM_VAR_InitializeRedis_redisConnect;
}

// 创建redis资源池
RedisPool* IAM_FUN_InitializeRedisPool(const AppConfig* config, int poolSize) {
    RedisPool* IAM_VAR_InitializeRedisPool_pool = (RedisPool*)malloc(sizeof(RedisPool));
    IAM_VAR_InitializeRedisPool_pool->IAM_GLV_redisConnections = (redisContext**)malloc(sizeof(redisContext*) * poolSize);
    IAM_VAR_InitializeRedisPool_pool->IAM_GLV_poolSize = poolSize;
    pthread_mutex_init(&IAM_VAR_InitializeRedisPool_pool->IAM_GLV_poolMutex, NULL);

    for (int i = 0; i < poolSize; i++) {
        IAM_VAR_InitializeRedisPool_pool->IAM_GLV_redisConnections[i] = IAM_FUN_InitializeRedis(config);
    }

    return IAM_VAR_InitializeRedisPool_pool;
}


// 检查redis是否可用，如果不可用则重新初始化redis连接
// 正常返回true，异常且重新初始化redis连接失败则停止服务运行
bool IAM_FUN_CheckAndReinitializeRedis(const AppConfig* IAM_VAR_CheckAndReinitializeRedis_config, int IAM_VAR_CheckAndReinitializeRedis_index) {
    // 使用互斥锁，让IAM_FUN_CheckAndReinitializeRedis永远单线程执行
    pthread_mutex_lock(&IAM_GLV_redisConnection_mutex);
    // 使用一个简单的命令来检查Redis连接
    if (IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_CheckAndReinitializeRedis_index] != NULL) {
        redisReply *reply = redisCommand(IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_CheckAndReinitializeRedis_index], "PING");
        if (reply != NULL && reply->type != REDIS_REPLY_ERROR) {
            freeReplyObject(reply);
            // 解除互斥锁
            pthread_mutex_unlock(&IAM_GLV_redisConnection_mutex);
            return true;
        }
        // 清理失败的回复
        if (reply != NULL) {
            freeReplyObject(reply);
        }
    }

    // 连接不可用，释放旧的连接
    if (IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_CheckAndReinitializeRedis_index] != NULL) {
        redisFree(IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_CheckAndReinitializeRedis_index]);
        IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_CheckAndReinitializeRedis_index] = NULL;
    }

    // 尝试重新初始化Redis连接
    IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_CheckAndReinitializeRedis_index] = IAM_FUN_InitializeRedis(IAM_VAR_CheckAndReinitializeRedis_config);
    if (IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_CheckAndReinitializeRedis_index] == NULL || IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_CheckAndReinitializeRedis_index]->err) {
        // 如果初始化失败，记录错误并终止服务
        dzlog_error("Failed to reinitialize Redis connection. Stopping service.");
        exit(EXIT_FAILURE);
    }

    // 解除互斥锁
    pthread_mutex_unlock(&IAM_GLV_redisConnection_mutex);
    return true;
}


// 随机生成token字符串
// 正确返回token字符串，唯一出错的可能就是malloc分配内存失败，那会返回NULL
char* IAM_FUN_GenerateRandomToken(int IAM_VAR_GenerateRandomToken_length) {
    static const char IAM_CON_alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    char* IAM_VAR_GenerateRandomToken_newToken = malloc(IAM_VAR_GenerateRandomToken_length + 1);

    if (IAM_VAR_GenerateRandomToken_newToken) {
        for (int i = 0; i < IAM_VAR_GenerateRandomToken_length; ++i) {
            IAM_VAR_GenerateRandomToken_newToken[i] = IAM_CON_alphanum[rand() % (sizeof(IAM_CON_alphanum) - 1)];
        }
        IAM_VAR_GenerateRandomToken_newToken[IAM_VAR_GenerateRandomToken_length] = '\0';
    }

    return IAM_VAR_GenerateRandomToken_newToken;
}


// 生成token并存储到redis
// 正确返回为token字符串，错误返回为NULL
char* IAM_FUN_GenerateAndStoreToken(const AppConfig* IAM_VAR_GenerateAndStoreToken_config, const char* IAM_VAR_GenerateAndStoreToken_serviceUsername, const char* IAM_VAR_GenerateAndStoreToken_requestId) {
    int IAM_VAR_AuthenticateRequest_index = rand() % 10;
    // 快速检查Redis连接状态
    if (IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_AuthenticateRequest_index] == NULL || IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_AuthenticateRequest_index]->err) {
        // 如果连接无效，调用IAM_FUN_CheckAndReinitializeRedis来尝试恢复连接
        if (!IAM_FUN_CheckAndReinitializeRedis(IAM_VAR_GenerateAndStoreToken_config, IAM_VAR_AuthenticateRequest_index)) {
            // 如果连接仍然无法恢复，处理错误
            dzlog_error("[%s]Redis operation failed: Redis connection cannot be established.", IAM_VAR_GenerateAndStoreToken_requestId);
            return NULL;
        }
    }

    char* IAM_VAR_GenerateAndStoreToken_newToken = IAM_FUN_GenerateRandomToken(IAM_CON_tokenLength);
    if (!IAM_VAR_GenerateAndStoreToken_newToken) {
        dzlog_error("[%s]Failed to generate token, service name: %s", IAM_VAR_GenerateAndStoreToken_requestId, IAM_VAR_GenerateAndStoreToken_serviceUsername);
        return NULL;
    }

    // 存储token到redis
    redisReply* reply = redisCommand(IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[IAM_VAR_AuthenticateRequest_index], "SETEX %s 1800 %s", IAM_VAR_GenerateAndStoreToken_serviceUsername, IAM_VAR_GenerateAndStoreToken_newToken);
    if (reply->type == REDIS_REPLY_ERROR) {
        dzlog_error("[%s]Error reported when writing service name %s to Redis: %s", IAM_VAR_GenerateAndStoreToken_requestId, IAM_VAR_GenerateAndStoreToken_serviceUsername, reply->str);
        freeReplyObject(reply);
        return NULL;
    } else {
        dzlog_debug("[%s]Token generated and stored successfully for service name: %s", IAM_VAR_GenerateAndStoreToken_requestId, IAM_VAR_GenerateAndStoreToken_serviceUsername);
    }
    
    freeReplyObject(reply);
    return(IAM_VAR_GenerateAndStoreToken_newToken);
}


// 加密验证，加密方式和请求服务传过来的鉴权加密方式相同
// 正确返回为一个加密后的十六进制字符串（service名+service密码+时间戳），错误返回为NULL
char* IAM_FUN_EncryptString(const char* IAM_VAR_EncryptString_serviceName, const char* IAM_VAR_EncryptString_servicePasswd, const char* IAM_VAR_EncryptString_timeStamp, const char* IAM_VAR_EncryptString_requestId) {
    // 计算输入字符串的长度并分配足够的空间，加2是因为两个加号
    size_t IAM_VAR_EncryptString_len = strlen(IAM_VAR_EncryptString_serviceName) + strlen(IAM_VAR_EncryptString_servicePasswd) + strlen(IAM_VAR_EncryptString_timeStamp) + 2;
    char* IAM_VAR_EncryptString_input = malloc(IAM_VAR_EncryptString_len + 1);
    if (IAM_VAR_EncryptString_input == NULL) {
        dzlog_error("[%s]IAM_FUN_EncryptString function memory allocation failed.", IAM_VAR_EncryptString_requestId);
        return NULL;
    }

    // 将三个字符串用加号连接
    sprintf(IAM_VAR_EncryptString_input, "%s+%s+%s", IAM_VAR_EncryptString_serviceName, IAM_VAR_EncryptString_servicePasswd, IAM_VAR_EncryptString_timeStamp);

    // 进行SHA-256加密
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)IAM_VAR_EncryptString_input, strlen(IAM_VAR_EncryptString_input), hash);
    free(IAM_VAR_EncryptString_input);

    // 将哈希值转换为十六进制字符串
    char* IAM_VAR_EncryptString_output = malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    if (IAM_VAR_EncryptString_output == NULL) {
        dzlog_error("[%s]IAM_FUN_EncryptString function memory allocation failed.", IAM_VAR_EncryptString_requestId);
        return NULL;
    }
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(IAM_VAR_EncryptString_output + i * 2, "%02x", hash[i]);
    }

    return IAM_VAR_EncryptString_output;
}


// 验证匹配，用encrypt_string函数进行相同加密方式验算，然后与请求服务发送来的加密字符串匹配
// 正确返回true，错误返回false
bool IAM_FUN_VerifyRequest(const char* IAM_VAR_VerifyRequest_encryptedStringToVerified, const char* IAM_VAR_VerifyRequest_serviceName, const char* IAM_VAR_VerifyRequest_servicePasswd, const char* IAM_VAR_VerifyRequest_timeStamp, const char* IAM_VAR_VerifyRequest_requestId) {
    // 使用服务名、密码和时间戳进行加密
    char* IAM_VAR_VerifyRequest_correctEncryptedString = IAM_FUN_EncryptString(IAM_VAR_VerifyRequest_serviceName, IAM_VAR_VerifyRequest_servicePasswd, IAM_VAR_VerifyRequest_timeStamp, IAM_VAR_VerifyRequest_requestId);
    if (IAM_VAR_VerifyRequest_correctEncryptedString == NULL) {
        dzlog_error("[%s]%s service user request for token, authentication failed\n", IAM_VAR_VerifyRequest_requestId, IAM_VAR_VerifyRequest_serviceName);
        return false;
    }

    bool IAM_VAR_VerifyRequest_isValid = strcmp(IAM_VAR_VerifyRequest_correctEncryptedString, IAM_VAR_VerifyRequest_encryptedStringToVerified) == 0;
    // 释放加密字符串
    free(IAM_VAR_VerifyRequest_correctEncryptedString);
    return IAM_VAR_VerifyRequest_isValid;
}


// 用于写入响应数据的函数
static size_t IAM_FUN_WriteResponse(void *IAM_VAR_WriteResponse_ptr, size_t IAM_VAR_WriteResponse_size, size_t IAM_VAR_WriteResponse_nmemb, char **IAM_VAR_WriteResponse_response) {
    size_t IAM_VAR_WriteResponse_newLen = strlen(*IAM_VAR_WriteResponse_response) + IAM_VAR_WriteResponse_size * IAM_VAR_WriteResponse_nmemb;
    *IAM_VAR_WriteResponse_response = realloc(*IAM_VAR_WriteResponse_response, IAM_VAR_WriteResponse_newLen + 1);
    if (*IAM_VAR_WriteResponse_response == NULL) {
        dzlog_error("IAM_FUN_WriteResponse realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(*IAM_VAR_WriteResponse_response + strlen(*IAM_VAR_WriteResponse_response), IAM_VAR_WriteResponse_ptr, IAM_VAR_WriteResponse_size * IAM_VAR_WriteResponse_nmemb);
    (*IAM_VAR_WriteResponse_response)[IAM_VAR_WriteResponse_newLen] = '\0';
    return IAM_VAR_WriteResponse_size * IAM_VAR_WriteResponse_nmemb;
}


// 从数据库服务获取申请token的service的密码
char* IAM_FUN_GetServicePassword(const char* IAM_VAR_GetServicePassword_serviceUrl, const char* IAM_VAR_GetServicePassword_token, const char* IAM_VAR_GetServicePassword_serviceName, const char* IAM_VAR_GetServicePassword_requestId) {
    json_error_t IAM_VAR_GetTokenHandler_dataJsonError;
    CURL *IAM_VAR_GetServicePassword_curl;
    CURLcode IAM_VAR_GetServicePassword_res;
    char *IAM_VAR_GetServicePassword_data = NULL;
    char *IAM_VAR_GetServicePassword_response = calloc(1, 1);

    // 初始化CURL会话
    IAM_VAR_GetServicePassword_curl = curl_easy_init();
    if (IAM_VAR_GetServicePassword_curl) {
        // 设置URL和POST数据
        char IAM_VAR_GetServicePassword_postData[256];
        sprintf(IAM_VAR_GetServicePassword_postData, "{\"service_username\": \"%s\"}", IAM_VAR_GetServicePassword_serviceName);
        curl_easy_setopt(IAM_VAR_GetServicePassword_curl, CURLOPT_URL, IAM_VAR_GetServicePassword_serviceUrl);
        curl_easy_setopt(IAM_VAR_GetServicePassword_curl, CURLOPT_POSTFIELDS, IAM_VAR_GetServicePassword_postData);

        // 设置自定义标头
        struct curl_slist *IAM_VAR_GetServicePassword_headers = NULL;
        char IAM_VAR_GetServicePassword_serviceNameHeader[256];
        char IAM_VAR_GetServicePassword_authHeader[256];
        sprintf(IAM_VAR_GetServicePassword_serviceNameHeader, "ServiceName: IAM_SERVICE");
        sprintf(IAM_VAR_GetServicePassword_authHeader, "Authorization: %s", IAM_VAR_GetServicePassword_token);
        IAM_VAR_GetServicePassword_headers = curl_slist_append(IAM_VAR_GetServicePassword_headers, IAM_VAR_GetServicePassword_serviceNameHeader);
        IAM_VAR_GetServicePassword_headers = curl_slist_append(IAM_VAR_GetServicePassword_headers, IAM_VAR_GetServicePassword_authHeader);
        IAM_VAR_GetServicePassword_headers = curl_slist_append(IAM_VAR_GetServicePassword_headers, "Content-Type: application/json");
        curl_easy_setopt(IAM_VAR_GetServicePassword_curl, CURLOPT_HTTPHEADER, IAM_VAR_GetServicePassword_headers);

        // 设置回调函数以处理响应
        curl_easy_setopt(IAM_VAR_GetServicePassword_curl, CURLOPT_WRITEFUNCTION, IAM_FUN_WriteResponse);
        curl_easy_setopt(IAM_VAR_GetServicePassword_curl, CURLOPT_WRITEDATA, &IAM_VAR_GetServicePassword_response);

        // 发送请求
        IAM_VAR_GetServicePassword_res = curl_easy_perform(IAM_VAR_GetServicePassword_curl);

        // 检查请求返回
        if (IAM_VAR_GetServicePassword_res != CURLE_OK) {
            dzlog_error("[%s]curl_easy_perform() failed: %s\n", IAM_VAR_GetServicePassword_requestId, curl_easy_strerror(IAM_VAR_GetServicePassword_res));
        } else {
            // 分析JSON响应
            json_t *IAM_VAR_GetServicePassword_parsedJson = json_loads(IAM_VAR_GetServicePassword_response, 0, &IAM_VAR_GetTokenHandler_dataJsonError);
            if (!IAM_VAR_GetServicePassword_parsedJson) {
                dzlog_error("[%s]IAM_FUN_GetServicePassword Error parsing JSON: %s",IAM_VAR_GetServicePassword_requestId, IAM_VAR_GetTokenHandler_dataJsonError.text);

                // 清理
                curl_slist_free_all(IAM_VAR_GetServicePassword_headers);
                curl_easy_cleanup(IAM_VAR_GetServicePassword_curl);
                free(IAM_VAR_GetServicePassword_response);
                return NULL;
            }

            json_t *IAM_VAR_GetServicePassword_servicePasswdJson = json_object_get(IAM_VAR_GetServicePassword_parsedJson, "ServicePassword");
            if (!json_is_string(IAM_VAR_GetServicePassword_servicePasswdJson)) {
                dzlog_error("[%s]Error: ServicePassword is not a string.", IAM_VAR_GetServicePassword_requestId);
                json_decref(IAM_VAR_GetServicePassword_parsedJson);
                curl_slist_free_all(IAM_VAR_GetServicePassword_headers);
                curl_easy_cleanup(IAM_VAR_GetServicePassword_curl);
                free(IAM_VAR_GetServicePassword_response);
                return NULL;
            }

            const char* IAM_VAR_GetServicePassword_servicePasswdChar = json_string_value(IAM_VAR_GetServicePassword_servicePasswdJson);
            IAM_VAR_GetServicePassword_data = strdup(IAM_VAR_GetServicePassword_servicePasswdChar);
            json_decref(IAM_VAR_GetServicePassword_parsedJson);
        }

        curl_slist_free_all(IAM_VAR_GetServicePassword_headers);
        curl_easy_cleanup(IAM_VAR_GetServicePassword_curl);
    }

    free(IAM_VAR_GetServicePassword_response);
    // 返回从数据库服务那获取的密码
    return IAM_VAR_GetServicePassword_data;
}


// 通过verify_request对请求服务进行鉴权，而后用generate_and_store_token进行token创建和存储
// 正确返回为token字符串，错误返回为"illegal_request"
char* IAM_FUN_InterfaceCore(const AppConfig* app_config, const char* IAM_VAR_InterfaceCore_reqAuthServiceName, const char* IAM_VAR_InterfaceCore_encryptedString, const char* IAM_VAR_InterfaceCore_timeStamp, const char* IAM_VAR_InterfaceCore_requestId) {
    // 确保app_config不为空
    if (app_config == NULL) {
        dzlog_error("[%s] App configuration is null.", IAM_VAR_InterfaceCore_requestId);
        return strdup("illegal_request");
    }
    // 获DBOP证服务配置
    char IAM_VAR_InterfaceCore_dbUrl[256];
    sprintf(IAM_VAR_InterfaceCore_dbUrl, "http://%s:%s/get_service_passwd", app_config->IAM_GLV_dbopServiceHost, app_config->IAM_GLV_dbopServicePort);

    // 与DBOP通信获取服务密码
    char* IAM_VAR_InterfaceCore_servicePasswd = IAM_FUN_GetServicePassword(IAM_VAR_InterfaceCore_dbUrl, IAM_GLV_selfUseToken, IAM_VAR_InterfaceCore_reqAuthServiceName, IAM_VAR_InterfaceCore_requestId);
    if (IAM_VAR_InterfaceCore_servicePasswd == NULL) {
        dzlog_error("[%s] Failed to get service password from C service.", IAM_VAR_InterfaceCore_requestId);
        return strdup("illegal_request");
    }

    // 验证请求
    if (IAM_FUN_VerifyRequest(IAM_VAR_InterfaceCore_encryptedString, IAM_VAR_InterfaceCore_reqAuthServiceName, IAM_VAR_InterfaceCore_servicePasswd, IAM_VAR_InterfaceCore_timeStamp, IAM_VAR_InterfaceCore_requestId)) {
        char* IAM_VAR_InterfaceCore_newToken = IAM_FUN_GenerateAndStoreToken(app_config, IAM_VAR_InterfaceCore_reqAuthServiceName, IAM_VAR_InterfaceCore_requestId);
        if (IAM_VAR_InterfaceCore_newToken == NULL) {
            free(IAM_VAR_InterfaceCore_servicePasswd);
            dzlog_error("[%s] illegal_request,Failed to generate and store IAM_VAR_GetServicePassword_token.", IAM_VAR_InterfaceCore_requestId);
            return strdup("illegal_request");
        }
        free(IAM_VAR_InterfaceCore_servicePasswd);
        return IAM_VAR_InterfaceCore_newToken;
    } else {
        free(IAM_VAR_InterfaceCore_servicePasswd);
        dzlog_error("[%s] illegal_request,Password verification failed.", IAM_VAR_InterfaceCore_requestId);
        return strdup("illegal_request");
    }
}


// 请求地址样例curl "http://192.168.1.1:10089/get_token/?service_username=billing&encrypt_str=1asdwn1jn2ezlflaw1nj231&date=2023-12-12%2010:39:07"
void IAM_FUN_GetTokenHandler(struct evhttp_request *IAM_VAR_GetTokenHandler_request, void *IAM_VAR_GetTokenHandler_voidCfg) {
    AppConfig *IAM_VAR_GetTokenHandler_cfg = (AppConfig *)IAM_VAR_GetTokenHandler_voidCfg;
    const char *IAM_VAR_GetTokenHandler_requestId = evhttp_find_header(evhttp_request_get_input_headers(IAM_VAR_GetTokenHandler_request), "X-Request-ID");
    char uuid_str[37];  // UUID字符串的长度
    if (!IAM_VAR_GetTokenHandler_requestId) {
        // 如果请求中没有X-Request-ID头部，生成一个UUID作为请求ID
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, uuid_str);
        IAM_VAR_GetTokenHandler_requestId = uuid_str;
    }
    dzlog_info("[%s]Processing API request to GetTokenHandler.", IAM_VAR_GetTokenHandler_requestId);
    // 从 POST 数据中读取 JSON 参数
    struct evbuffer *IAM_VAR_GetTokenHandler_inputBuffer = evhttp_request_get_input_buffer(IAM_VAR_GetTokenHandler_request);
    size_t IAM_VAR_GetTokenHandler_bufferLen = evbuffer_get_length(IAM_VAR_GetTokenHandler_inputBuffer);
    char IAM_VAR_GetTokenHandler_postData[IAM_VAR_GetTokenHandler_bufferLen + 1];
    evbuffer_remove(IAM_VAR_GetTokenHandler_inputBuffer, IAM_VAR_GetTokenHandler_postData, IAM_VAR_GetTokenHandler_bufferLen);
    IAM_VAR_GetTokenHandler_postData[IAM_VAR_GetTokenHandler_bufferLen] = '\0';

    json_error_t IAM_VAR_GetTokenHandler_dataJsonError;
    json_t *IAM_VAR_GetTokenHandler_dataJsonAll = json_loads(IAM_VAR_GetTokenHandler_postData, 0, &IAM_VAR_GetTokenHandler_dataJsonError);

    dzlog_info("[%s]Parsing POST data.", IAM_VAR_GetTokenHandler_requestId);
    if (!IAM_VAR_GetTokenHandler_dataJsonAll) {
        dzlog_error("Failed to parse JSON from request body, request id: %s \n Error parsing JSON: %s", IAM_VAR_GetTokenHandler_requestId, IAM_VAR_GetTokenHandler_dataJsonError.text);
        evhttp_send_reply(IAM_VAR_GetTokenHandler_request, 400, "Bad Request", NULL);
        return;
    }
    json_t *IAM_VAR_GetTokenHandler_serviceUsernameJson = json_object_get(IAM_VAR_GetTokenHandler_dataJsonAll, "service_username");
    json_t *IAM_VAR_GetTokenHandler_encryptStringJson = json_object_get(IAM_VAR_GetTokenHandler_dataJsonAll, "encrypt_str");
    json_t *IAM_VAR_GetTokenHandler_timestampJson = json_object_get(IAM_VAR_GetTokenHandler_dataJsonAll, "date");

    const char *IAM_VAR_GetTokenHandler_serviceUsername = json_is_string(IAM_VAR_GetTokenHandler_serviceUsernameJson) ? json_string_value(IAM_VAR_GetTokenHandler_serviceUsernameJson) : NULL;
    const char *IAM_VAR_GetTokenHandler_encryptString = json_is_string(IAM_VAR_GetTokenHandler_encryptStringJson) ? json_string_value(IAM_VAR_GetTokenHandler_encryptStringJson) : NULL;
    const char *IAM_VAR_GetTokenHandler_timestamp = json_is_string(IAM_VAR_GetTokenHandler_timestampJson) ? json_string_value(IAM_VAR_GetTokenHandler_timestampJson) : NULL;

    // 检查是否所有需要的参数都已经正确获取
    if (!IAM_VAR_GetTokenHandler_serviceUsername || !IAM_VAR_GetTokenHandler_encryptString || !IAM_VAR_GetTokenHandler_timestamp) {
        // 处理错误：某些参数未获取或不是字符串
        json_decref(IAM_VAR_GetTokenHandler_dataJsonAll);
        return;
    }

    char* IAM_VAR_GetTokenHandler_authResult = IAM_FUN_InterfaceCore(IAM_VAR_GetTokenHandler_cfg, IAM_VAR_GetTokenHandler_serviceUsername, IAM_VAR_GetTokenHandler_encryptString, IAM_VAR_GetTokenHandler_timestamp, IAM_VAR_GetTokenHandler_requestId);
    if (strcmp(IAM_VAR_GetTokenHandler_authResult, "illegal_request") != 0) {
        struct evbuffer *IAM_VAR_GetTokenHandler_buffer = evbuffer_new();
        if (IAM_VAR_GetTokenHandler_buffer) {
            evbuffer_add_printf(IAM_VAR_GetTokenHandler_buffer, "{\"Token\":\"%s\"}", IAM_VAR_GetTokenHandler_authResult);
            evhttp_send_reply(IAM_VAR_GetTokenHandler_request, HTTP_OK, "OK", IAM_VAR_GetTokenHandler_buffer);
            evbuffer_free(IAM_VAR_GetTokenHandler_buffer);
        }
        free(IAM_VAR_GetTokenHandler_authResult);
    } else {
        evhttp_send_reply(IAM_VAR_GetTokenHandler_request, 400, "Request verification failed", NULL);
        free(IAM_VAR_GetTokenHandler_authResult);
    }
}



// 触发器，去触发generate_and_store_token来生成IAM自身服务的token
void IAM_FUN_TokenRefreshCallback(evutil_socket_t IAM_VAR_TokenRefreshCallback_socketFd, short IAM_VAR_TokenRefreshCallback_eventType, void *arg) {
    AppConfig *app_config = (AppConfig *)arg;
    char* IAM_VAR_TokenRefreshCallback_newToken = IAM_FUN_GenerateAndStoreToken(app_config, "IAM_SERVICE", "TOKEN_REFRESH");

    if (IAM_VAR_TokenRefreshCallback_newToken == NULL) {
        fprintf(stderr, "Failed to generate and store token. Stopping service.\n");
        // 停止事件循环，这将导致 main 函数中的 event_base_dispatch 返回
        event_base_loopexit(event_get_base(IAM_GLV_tokenRefreshEvent), NULL);
        return;
    }

    // 释放旧的 IAM_GLV_selfUseToken 内存
    if (IAM_GLV_selfUseToken != NULL) {
        free(IAM_GLV_selfUseToken);
    }

    // 将新生成的 IAM_VAR_TokenRefreshCallback_newToken 赋值给 IAM_GLV_selfUseToken
    IAM_GLV_selfUseToken = IAM_VAR_TokenRefreshCallback_newToken;
}


int main() {
    struct evhttp *IAM_VAR_Main_HttpServerInstance;
    struct evhttp_bound_socket *IAM_VAR_Main_HttpSocket;
    struct timeval IAM_VAR_Main_TokenRefreshTime = {25 * 60, 0};
    struct event_base *IAM_VAR_Main_eventBase = event_base_new();
    struct evhttp *IAM_VAR_Main_httpServer = evhttp_new(IAM_VAR_Main_eventBase);

    // 初始化随机数种子
    srand(time(NULL));

    AppConfig IAM_VAR_Main_cfg = IAM_FUN_MainConfigParse("config/iam_config.yaml");
    IAM_GLV_redisConnectPool = IAM_FUN_InitializeRedisPool(&IAM_VAR_Main_cfg, 10);
    
    IAM_FUN_InitLogging(&IAM_VAR_Main_cfg);

    evhttp_set_cb(IAM_VAR_Main_httpServer, "/get_token", IAM_FUN_GetTokenHandler, &IAM_VAR_Main_cfg);
    // 初始化libevent的多线程支持
    evthread_use_pthreads();

    // 初始化libcurl的多线程支持
    curl_global_init(CURL_GLOBAL_ALL);

    // 先生成一个自己的token
    IAM_FUN_TokenRefreshCallback(-1, 0, &IAM_VAR_Main_cfg);
    // 每隔token_refresh_time分钟就会去重新生成一个自己的token
    IAM_GLV_tokenRefreshEvent = event_new(IAM_VAR_Main_eventBase, -1, EV_PERSIST, IAM_FUN_TokenRefreshCallback, &IAM_VAR_Main_cfg);
    evtimer_add(IAM_GLV_tokenRefreshEvent, &IAM_VAR_Main_TokenRefreshTime);

    if (evhttp_bind_socket(IAM_VAR_Main_httpServer, "0.0.0.0", atoi(IAM_VAR_Main_cfg.IAM_GLV_serverPort)) != 0) {
        // 这里后面要补错误处理逻辑
        // watting to do
        dzlog_error("Failed to bind socket to port %s. Exiting.", IAM_VAR_Main_cfg.IAM_GLV_serverPort);
        
        // 释放资源
        if (IAM_GLV_tokenRefreshEvent) {
            event_free(IAM_GLV_tokenRefreshEvent);
        }
        
        evhttp_free(IAM_VAR_Main_httpServer);
        event_base_free(IAM_VAR_Main_eventBase);
        
        // 释放Redis连接池资源
        if (IAM_GLV_redisConnectPool) {
            for (int i = 0; i < IAM_GLV_redisConnectPool->IAM_GLV_poolSize; i++) {
                if (IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[i]) {
                    redisFree(IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[i]);
                }
            }
            free(IAM_GLV_redisConnectPool->IAM_GLV_redisConnections);
            pthread_mutex_destroy(&IAM_GLV_redisConnectPool->IAM_GLV_poolMutex);
            free(IAM_GLV_redisConnectPool);
        }
        
        // 释放配置结构体中的字符串成员
        free(IAM_VAR_Main_cfg.IAM_GLV_redisServerIp);
        free(IAM_VAR_Main_cfg.IAM_GLV_redisServerPort);
        free(IAM_VAR_Main_cfg.IAM_GLV_redisServerPassword);
        free(IAM_VAR_Main_cfg.IAM_GLV_dbopServiceHost);
        free(IAM_VAR_Main_cfg.IAM_GLV_dbopServicePort);
        free(IAM_VAR_Main_cfg.IAM_GLV_serverIp);
        free(IAM_VAR_Main_cfg.IAM_GLV_serverPort);
        free(IAM_VAR_Main_cfg.IAM_GLV_logConfig);
        
        // 释放全局token
        if (IAM_GLV_selfUseToken) {
            free(IAM_GLV_selfUseToken);
        }
        
        // 销毁互斥锁
        pthread_mutex_destroy(&IAM_GLV_redisConnection_mutex);
        
        // 关闭日志系统
        zlog_fini();
        
        // 清理libcurl资源
        curl_global_cleanup();
        
        return 1;
    }

    // 启动事件循环
    event_base_dispatch(IAM_VAR_Main_eventBase);

    // 释放 libevent 和 libevhttp 资源
    evhttp_free(IAM_VAR_Main_httpServer);
    event_base_free(IAM_VAR_Main_eventBase);

    if (IAM_GLV_tokenRefreshEvent) {
        event_free(IAM_GLV_tokenRefreshEvent);
    }

    // 释放Redis连接池资源
    if (IAM_GLV_redisConnectPool) {
        for (int i = 0; i < IAM_GLV_redisConnectPool->IAM_GLV_poolSize; i++) {
            if (IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[i]) {
                redisFree(IAM_GLV_redisConnectPool->IAM_GLV_redisConnections[i]);
            }
        }
        free(IAM_GLV_redisConnectPool->IAM_GLV_redisConnections);
        pthread_mutex_destroy(&IAM_GLV_redisConnectPool->IAM_GLV_poolMutex);
        free(IAM_GLV_redisConnectPool);
    }

    // 释放配置结构体中的字符串成员
    free(IAM_VAR_Main_cfg.IAM_GLV_redisServerIp);
    free(IAM_VAR_Main_cfg.IAM_GLV_redisServerPort);
    free(IAM_VAR_Main_cfg.IAM_GLV_redisServerPassword);
    free(IAM_VAR_Main_cfg.IAM_GLV_dbopServiceHost);
    free(IAM_VAR_Main_cfg.IAM_GLV_dbopServicePort);
    free(IAM_VAR_Main_cfg.IAM_GLV_serverIp);
    free(IAM_VAR_Main_cfg.IAM_GLV_serverPort);
    free(IAM_VAR_Main_cfg.IAM_GLV_logConfig);

    // 释放全局token
    if (IAM_GLV_selfUseToken) {
        free(IAM_GLV_selfUseToken);
    }

    // 销毁互斥锁
    pthread_mutex_destroy(&IAM_GLV_redisConnection_mutex);

    // 关闭日志系统
    zlog_fini();

    // 清理libcurl资源
    curl_global_cleanup();

    return 0;
}