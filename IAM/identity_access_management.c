// 整体目的：用以给服务间通讯的各个组件生成请求用token，对内发布socket
// 访问方式样例：curl -X GET "http://192.168.1.1:10089/get_token/?service_username=billing&encrypt_str=1asdwn1jn2ezlflaw1nj231&date=2023-12-12%2010:39:07"
#include <stdio.h>
#include <yaml.h>
#include <hiredis/hiredis.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/sha.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <pthread.h>
// libvent多线程支持头文件
// #include <event2/thread.h>

// 目前为单线程、http
// 代码结构支持多线程场景，必要时只需要解除多线程支持的注释即可
// 未来要改为https
// 目前错误日志为标准错误输出来测试，后续要用log4c
// 大部分注释为通过ChatGPT4添加，不保证准确性，仅供参考，后续会手工写下本文件注释，并由ChatGPT4生成一个逐行注释版，类似database的那个注释版用于各位参考。
// 后续会规范下变量名，重新调整下，目前仅做功能实现


#define TOKEN_LENGTH 32 
char* global_b_service_token = NULL; 
redisContext* redis_ctx = NULL;
pthread_mutex_t redis_ctx_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct RedisConfig {
    char* server;
    char* port;
    char* password;
} RedisConfig;

typedef struct DBOPServiceConfig {
    char* url;
    char* port;
} DBOPServiceConfig;

typedef struct {
    RedisConfig redis_config;
    DBOPServiceConfig dbop_service_config;
    char* server_ip;
    char* server_port;
} AppConfig;


// 函数来解析配置文件
// 正确返回为config字典，错误返回为NULL，或终止服务
AppConfig parse_config(const char* filename) {
    FILE *fh = fopen(filename, "r");
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fprintf(stderr, "Failed to initialize parser\n");
        exit(EXIT_FAILURE);
    }
    if (fh == NULL) {
        fprintf(stderr, "Failed to open file\n");
        exit(EXIT_FAILURE);
    }
    yaml_parser_set_input_file(&parser, fh);

    AppConfig config = {0};
    char* current_key = NULL;

    // 解析YAML文件
    bool done = false;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Parser error\n");
            exit(EXIT_FAILURE);
        }

        switch(event.type) {
        case YAML_SCALAR_EVENT:
            if (current_key) {
                if (strcmp(current_key, "redis_server") == 0) {
                    config.redis_config.server = strdup((char*)event.data.scalar.value);
                } else if (strcmp(current_key, "redis_port") == 0) {
                    config.redis_config.port = strdup((char*)event.data.scalar.value);
                } else if (strcmp(current_key, "redis_passwd") == 0) {
                    config.redis_config.password = strdup((char*)event.data.scalar.value);
                } else if (strcmp(current_key, "dbop_service_url") == 0) {
                    config.dbop_service_config.url = strdup((char*)event.data.scalar.value);
                } else if (strcmp(current_key, "dbop_service_port") == 0) {
                    config.dbop_service_config.port = strdup((char*)event.data.scalar.value);
                } else if (strcmp(current_key, "server_ip") == 0) {
                    config.server_ip = strdup((char*)event.data.scalar.value);
                } else if (strcmp(current_key, "server_port") == 0) {
                    config.server_port = strdup((char*)event.data.scalar.value);
                }
                free(current_key);
                current_key = NULL;
            } else {
                current_key = strdup((char*)event.data.scalar.value);
            }
            break;
        case YAML_STREAM_END_EVENT:
            done = true;
            break;
        default:
            break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);

    return config;
}


// 函数来初始化和连接到Redis
// 正确返回为redis_connect的连接，错位返回为NULL
redisContext* initialize_redis(const RedisConfig* config) {
    char* endptr;
    long port = strtol(config->port, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid port number: Non-numeric characters present\n");
        return NULL;
    }
    redisContext *redis_connect = redisConnect(config->server, (int)port);
    if (redis_connect == NULL || redis_connect->err) {
        if (redis_connect) {
            fprintf(stderr, "Redis connection error: %s\n", redis_connect->errstr);
            redisFree(redis_connect);
        } else {
            fprintf(stderr, "Connection error: can't allocate redis context\n");
        }
        return NULL;
    }

    // 如果提供了密码，则使用它进行认证
    if (config->password != NULL && strlen(config->password) > 0) {
        redisReply *reply = redisCommand(redis_connect, "AUTH %s", config->password);
        if (reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "Auth error: %s\n", reply->str);
            freeReplyObject(reply);
            redisFree(redis_connect);
            return NULL;
        }
        freeReplyObject(reply);
    }

    return redis_connect;
}


// 检查redis是否可用，如果不可用则重新初始化redis连接
// 正常返回true，异常且重新初始化redis连接失败则停止服务运行
bool check_and_reinitialize_redis(AppConfig *app_config) {
    // 使用互斥锁，让check_and_reinitialize_redis永远单线程执行
    pthread_mutex_lock(&redis_ctx_mutex);
    // 使用一个简单的命令来检查Redis连接
    if (redis_ctx != NULL) {
        redisReply *reply = redisCommand(redis_ctx, "PING");
        if (reply != NULL && reply->type != REDIS_REPLY_ERROR) {
            freeReplyObject(reply);
            // 解除互斥锁
            pthread_mutex_unlock(&redis_ctx_mutex);
            return true;
        }
        // 清理失败的回复
        if (reply != NULL) {
            freeReplyObject(reply);
        }
    }

    // 连接不可用，释放旧的连接
    if (redis_ctx != NULL) {
        redisFree(redis_ctx);
        redis_ctx = NULL;
    }

    // 尝试重新初始化Redis连接
    redis_ctx = initialize_redis(&app_config->redis_config);
    if (redis_ctx == NULL || redis_ctx->err) {
        // 如果初始化失败，记录错误并终止服务
        fprintf(stderr, "Failed to reinitialize Redis connection. Stopping service.\n");
        exit(EXIT_FAILURE);
    }

    // 解除互斥锁
    pthread_mutex_unlock(&redis_ctx_mutex);
    return true;
}


// 随机生成token字符串
// 正确返回token字符串，唯一出错的可能就是malloc分配内存失败，那会返回NULL
char* generate_random_token(int length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    char* token = malloc(length + 1);

    if (token) {
        for (int i = 0; i < length; ++i) {
            token[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
        }
        token[length] = '\0';
    }

    return token;
}


// 生成token并存储到redis
// 正确返回为token字符串，错误返回为NULL
char* generate_and_store_token(const AppConfig* app_config, const char* service_username) {
    // 快速检查Redis连接状态
    if (redis_ctx == NULL || redis_ctx->err) {
        // 如果连接无效，调用check_and_reinitialize_redis来尝试恢复连接
        if (!check_and_reinitialize_redis(app_config)) {
            // 如果连接仍然无法恢复，处理错误
            fprintf(stderr, "Redis operation failed: Redis connection cannot be established.\n");
            return;
        }
    }

    char* token = generate_random_token(TOKEN_LENGTH);
    if (!token) {
        fprintf(stderr, "Failed to generate token.\n");
        return NULL;
    }

    // 存储token到redis
    redisReply* reply = redisCommand(redis_ctx, "SETEX token:%s 1800 %s", service_username, token);
    if (reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "Redis error: %s\n", reply->str);
        free(reply);
        return NULL;
    } else {
        printf("Token generated and stored successfully for %s: %s\n", service_username, token);
    }
    
    free(reply);
    return(token);
}


// 加密验证，加密方式和请求服务传过来的鉴权加密方式相同
// 正确返回为一个加密后的十六进制字符串（service名+service密码+时间戳），错误返回为NULL
char* encrypt_string(const char* service_name, const char* service_passwd, const char* timestamp) {
    // 计算输入字符串的长度并分配足够的空间，加2是因为两个加号
    size_t len = strlen(service_name) + strlen(service_passwd) + strlen(timestamp) + 2;
    char* input = malloc(len + 1);
    if (input == NULL) {
        fprintf(stderr, "encrypt_string function memory allocation failed\n");
        return NULL;
    }

    // 将三个字符串用加号连接
    sprintf(input, "%s+%s+%s", service_name, service_passwd, timestamp);

    // 进行SHA-256加密
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input, strlen(input), hash);
    free(input);

    // 将哈希值转换为十六进制字符串
    char* output = malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    if (output == NULL) {
        fprintf(stderr, "encrypt_string function memory allocation failed\n");
        return NULL;
    }
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + i * 2, "%02x", hash[i]);
    }

    return output;
}


// 验证匹配，用encrypt_string函数进行相同加密方式验算，然后与请求服务发送来的加密字符串匹配
// 正确返回true，错误返回false
bool verify_request(const char* encrypted_string_from_A, const char* service_name, const char* service_passwd, const char* timestamp) {
    // 使用服务名、密码和时间戳进行加密
    char* encrypted = encrypt_string(service_name, service_passwd, timestamp);
    if (encrypted == NULL) {
        fprintf(stderr, "%s service user request for token, authentication failed\n", service_name);
        return false;
    }

    bool is_valid = strcmp(encrypted, encrypted_string_from_A) == 0;
    // 释放加密字符串
    free(encrypted);
    return is_valid;
}


// 用于写入响应数据的函数
static size_t write_response(void *ptr, size_t size, size_t nmemb, char **response) {
    size_t new_len = strlen(*response) + size * nmemb;
    *response = realloc(*response, new_len + 1);
    if (*response == NULL) {
        fprintf(stderr, "write_response realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(*response + strlen(*response), ptr, size * nmemb);
    (*response)[new_len] = '\0';
    return size * nmemb;
}


// 从数据库服务获取申请token的service的密码
char* get_service_password(const char* service_url, const char* token, const char* service_name) {
    CURL *curl;
    CURLcode res;
    char *data = NULL;
    char *response = calloc(1, 1);

    // 初始化CURL会话
    curl = curl_easy_init();
    if (curl) {
        // 设置URL和POST数据
        char post_data[256];
        sprintf(post_data, "service_name=%s", service_name);
        curl_easy_setopt(curl, CURLOPT_URL, service_url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

        // 设置自定义标头
        struct curl_slist *headers = NULL;
        char auth_header[256];
        sprintf(auth_header, "Authorization: %s", token);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 设置回调函数以处理响应
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // 发送请求
        res = curl_easy_perform(curl);

        // 检查请求返回
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // 分析JSON响应
            struct json_object *parsed_json;
            struct json_object *service_passwd;

            parsed_json = json_tokener_parse(response);
            if (!json_object_object_get_ex(parsed_json, "service_passwd", &service_passwd)) {
                fprintf(stderr, "get_service_password failed: %s No corresponding password found\n", service_name);

                // 清理
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                free(response);
                return NULL;
            }

            data = strdup(json_object_get_string(service_passwd));
            json_object_put(parsed_json);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(response);
    // 返回从数据库服务那获取的密码
    return data;
}


// 通过verify_request对请求服务进行鉴权，而后用generate_and_store_token进行token创建和存储
// 正确返回为token字符串，错误返回为"illegal_request"
char* handle_request_from_A_service(const AppConfig* app_config, const char* service_username, const char* encrypted_string, const char* timestamp) {
    // 确保app_config不为空
    if (app_config == NULL) {
        fprintf(stderr, "App configuration is null.\n");
        return strdup("illegal_request");
    }
    // 获取C服务配置
    char service_url[256];
    sprintf(service_url, "http://%s:%s/get_service_passwd", app_config->dbop_service_config.url, app_config->dbop_service_config.port);

    // 与C服务通信获取服务密码
    char* service_passwd = get_service_password(service_url, global_b_service_token, service_username);
    if (service_passwd == NULL) {
        free(service_passwd);
        fprintf(stderr, "Failed to get service password from C service.\n");
        return strdup("illegal_request");
    }

    // 验证请求
    if (verify_request(encrypted_string, service_passwd, timestamp)) {
        char* new_token = generate_and_store_token(app_config, service_username);
        if (new_token == NULL) {
            free(service_passwd);
            fprintf(stderr, "illegal_request,Failed to generate and store token.\n");
            return strdup("illegal_request");
        }
        free(service_passwd);
        return new_token;
    } else {
        free(service_passwd);
        fprintf(stderr, "illegal_request,Password verification failed.\n");
        return strdup("illegal_request");
    }
}


// 请求地址样例curl "http://192.168.1.1:10089/get_token/?service_username=billing&encrypt_str=1asdwn1jn2ezlflaw1nj231&date=2023-12-12%2010:39:07"
void get_token_handler(struct evhttp_request *req, void *arg) {
     AppConfig* app_config = (AppConfig*)arg;
    struct evkeyvalq params;
    const char *service_username = NULL;
    const char *encrypt_str = NULL;
    const char *date_time = NULL;

    // 获取请求的URI
    const char* uri = evhttp_request_get_uri(req);

    // 解析URI以获取路径
    struct evhttp_uri *decoded_uri = evhttp_uri_parse(uri);
    if (!decoded_uri) {
        // 如果URI解析失败，发送错误响应
        evhttp_send_reply(req, HTTP_BADREQUEST, "Bad Request", NULL);
        free(decoded_uri);
        return;
    }

    // 检查路径是否为"/get_token/"
    const char* path = evhttp_uri_get_path(decoded_uri);
    if (path == NULL || strcmp(path, "/get_token/") != 0) {
        // 如果路径不匹配，发送错误响应
        evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", NULL);
        evhttp_uri_free(decoded_uri);
        free(decoded_uri);
        return;
    }

    // 解析URL参数
    evhttp_parse_query(uri, &params);
    service_username = evhttp_find_header(&params, "service_username");
    encrypt_str = evhttp_find_header(&params, "encrypt_str");
    date_time = evhttp_find_header(&params, "date");

    // 调用业务逻辑
    char* result = handle_request_from_A_service(app_config, service_username, encrypt_str, date_time);

    if (strcmp(result, "illegal_request") != 0) {
        // 设置成功响应
        struct evbuffer *buf = evbuffer_new();
        if (buf) {
            evbuffer_add_printf(buf, "Token: %s", result);
            evhttp_send_reply(req, HTTP_OK, "OK", buf);
            evbuffer_free(buf);
        }
        free(result);
    } else {
        // 设置错误响应
        evhttp_send_reply(req, HTTP_BAD_REQUEST, "Request verification failed", NULL);
        free(result);
    }
}


// 触发器，去触发generate_and_store_token来生成IAM自身服务的token
void token_refresh_callback(evutil_socket_t fd, short event, void *arg) {
    AppConfig *app_config = (AppConfig *)arg;
    char* token = generate_and_store_token(app_config, "IAM_SERVICE");

    if (token == NULL) {
        fprintf(stderr, "Failed to generate and store token. Stopping service.\n");
        // 停止事件循环，这将导致 main 函数中的 event_base_dispatch 返回
        event_base_loopexit(event_get_base(fd), NULL);
        return;
    }

    // 释放旧的 global_b_service_token 内存
    if (global_b_service_token != NULL) {
        free(global_b_service_token);
    }

    // 将新生成的 token 赋值给 global_b_service_token
    global_b_service_token = token;
}


int main() {
    struct event_base *base;
    struct evhttp *http;
    struct evhttp_bound_socket *handle;
    struct event *token_refresh_event;
    struct timeval token_refresh_time = {25 * 60, 0}; 

    // 初始化随机数种子
    srand(time(NULL));  

    AppConfig app_config = parse_config("config/iam_config.yaml");
    redis_ctx = initialize_redis(&app_config.redis_config);
    
    // 初始化libevent的多线程支持
    // evthread_use_pthreads();
    
    // 初始化libcurl的多线程支持
    // curl_global_init(CURL_GLOBAL_ALL);

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }

    http = evhttp_new(base);
    if (!http) {
        fprintf(stderr, "Could not create evhttp. Exiting.\n");
        return 1;
    }

    // 每隔token_refresh_time分钟就会去重新生成一个自己的token
    token_refresh_event = event_new(base, -1, EV_PERSIST, token_refresh_callback, &app_config);
    evtimer_add(token_refresh_event, &token_refresh_time);

    evhttp_set_gencb(http, get_token_handler, NULL);

    handle = evhttp_bind_socket_with_handle(http, app_config.server_ip, atoi(app_config.server_port));
    if (!handle) {
        fprintf(stderr, "Could not bind to port %s.\n", app_config.server_port);
        return 1;
    }

    event_base_dispatch(base);

    event_free(token_refresh_event);
    evhttp_free(http);
    event_base_free(base);

    if (redis_ctx) {
        redisFree(redis_ctx);
    }

    // 清理libcurl资源(不是我想加的，我感觉没必要，这个是让gpt加注释的时候她补上的)
    curl_global_cleanup();

    return 0;
}