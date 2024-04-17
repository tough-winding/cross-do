// 整体目的：控制数据库的增删改查操作，均由该服务提供，对外发布socket服务
// 访问方式为 curl -X POST "http://1.1.1.1:19000/add_user_from_pool" -H "Authorization: This_is_TOKENTOKENTOKEN" -H "Content-Type: application/json" -d '{"user":"test_user", "password":"12345"}'
// add_user_from_pool函数的输入参数为user password 两个字符串类型
// 以下注释均由ChatGPT4生成，不保证准确性，仅供参考
// 目前该注释版和最新代码存在DIFF，后续定版后再重新生成该注释版

#include <stdio.h>      // 标准输入输出库，例如 printf, scanf 等
#include <stdlib.h>     // 标准库，例如 malloc, free 等
#include <string.h>     // 字符串操作库，例如 strcpy, strcmp 等
#include <yaml.h>       // YAML 解析库
#include <mysql/mysql.h> // MySQL 数据库库
#include <time.h>       // 时间库，例如 time, localtime 等
#include <log4c.h>      // log4c 日志库
#include <event2/event.h> // libevent 事件处理库
#include <event2/http.h>  // libevent 的 HTTP 功能
#include <event2/keyvalq_struct.h> // libevent 的键值对结构
#include <string.h>


//声明结构体，用以存储常量
struct db_config {
    char *server_port;
    char *log_file;
    char *log_config;
    char *database_ip;
    char *database_port;
    char *database_privilege_user;
    char *database_privilege_password;
    char *user_database_name;
};


// 声明一个和db_config相同的字符串数组，方便后续load_config部分用以检查和释放内存
// 在db_config这个结构体里新增的都要加到这里
const char *db_config_keys[] = {
    "server_port",
    "log_file",
    "log_config",
    "database_ip",
    "database_port",
    "database_privilege_user",
    "database_privilege_password",
    "user_database_name",
    NULL // 末尾的NULL，用于数组遍历结束标志
};


// // 用于检查一个给定的键 key 是否存在于 db_config_keys 数组中。
int is_known_key(const char *key) {
    // 遍历 db_config_keys 数组
    for (int i = 0; db_config_keys[i] != NULL; i++) {
        // 使用 strcmp 函数比较传入的 key 和数组中的每个键
        if (strcmp(key, db_config_keys[i]) == 0) {
            // 如果找到匹配的键，返回 1
            return 1;
        }
    }
    // 如果遍历完数组都没有找到匹配的键，返回 0
    return 0;
}


// init_logging 函数用于初始化 log4c 日志库。它使用 db_config 结构体中的 log_file 字段作为 log4c 的配置文件路径。
void init_logging(struct db_config *cfg) {
    // 尝试初始化 log4c 和加载配置文件
    if (log4c_init() == -1 || log4c_load_config(cfg->log_file) == -1) {
        // 如果初始化或加载配置失败，输出错误信息并退出程序
        fprintf(stderr, "Could not initialize log4c or load its config. Exiting.\n");
        exit(EXIT_FAILURE);
    }
}


// 读取error日志的配置
void log_error(const char *message) {
    log4c_category_log(log4c_category_get("database_operation_interface.error"), LOG4C_PRIORITY_ERROR, "%s", message);
}


// load_config函数作用是读取配置文件，并赋值
int load_config(struct db_config *cfg) {
    // 打开YAML配置文件
    FILE *database_operation_interface_conf = fopen("config/config.yaml", "r");
    // 如果文件打开失败，返回0
    if (database_operation_interface_conf == NULL) {
        return 0;
    }
    // 声明YAML解析器和事件变量
    yaml_parser_t parser;
    yaml_event_t event;

    // 初始化YAML解析器
    if (!yaml_parser_initialize(&parser)) {
        // 如果初始化失败，关闭文件并返回0
        fclose(database_operation_interface_conf);
        return 0;
    }

    // 将解析器输入设置为打开的文件
    yaml_parser_set_input_file(&parser, database_operation_interface_conf);

    // 初始化状态变量和当前键变量
    int in_key = 0;
    char *current_key = NULL;

    // 开始解析YAML文件
    while (1) {
        // 获取下一个YAML事件
        yaml_parser_parse(&parser, &event);
        
        // 如果到达文件尾部，跳出循环
        if (event.type == YAML_STREAM_END_EVENT) {
            break;
        }

        // 如果事件是标量事件（即键或值）
        if (event.type == YAML_SCALAR_EVENT) {
            // 如果处在键模式
            if (in_key) {
                // 获取当前值，并将其赋给相应的配置字段
                char *value = (char *)event.data.scalar.value;
                // 下面是一系列的键值对检查和赋值操作
                if (strcmp(current_key, "server_port") == 0) {
                    cfg->server_port = strdup(value);
                    if (cfg->server_port == NULL) {
                        log_error("Memory allocation failed");
                        exit(EXIT_FAILURE);
                    }
                } else if (strcmp(current_key, "log_file") == 0) {
                    cfg->log_file = strdup(value);
                    if (cfg->log_file == NULL) {
                        log_error("Memory allocation failed");
                        exit(EXIT_FAILURE);
                    }
                } else if (strcmp(current_key, "log_config") == 0) {
                    cfg->log_config = strdup(value);
                    if (cfg->log_config == NULL) {
                        log_error("Memory allocation failed");
                        exit(EXIT_FAILURE);
                    }
                } else if (strcmp(current_key, "database_ip") == 0) {
                    cfg->database_ip = strdup(value);
                    if (cfg->database_ip == NULL) {
                        log_error("Memory allocation failed");
                        exit(EXIT_FAILURE);
                    }
                } else if (strcmp(current_key, "database_port") == 0) {
                    cfg->database_port = strdup(value);
                    if (cfg->database_port == NULL) {
                        log_error("Memory allocation failed");
                        exit(EXIT_FAILURE);
                    }
                } else if (strcmp(current_key, "database_privilege_user") == 0) {
                    cfg->database_privilege_user = strdup(value);
                    if (cfg->database_privilege_user == NULL) {
                        log_error("Memory allocation failed");
                        exit(EXIT_FAILURE);
                    }
                } else if (strcmp(current_key, "database_privilege_password") == 0) {
                    cfg->database_privilege_password = strdup(value);
                    if (cfg->database_privilege_password == NULL) {
                        log_error("Memory allocation failed");
                        exit(EXIT_FAILURE);
                    }
                } else if (strcmp(current_key, "user_database_name") == 0) {
                    cfg->user_database_name = strdup(value);
                    if (cfg->user_database_name == NULL) {
                        log_error("Memory allocation failed");
                        exit(EXIT_FAILURE);
                    }
                }
                // 释放当前键，并将状态变量设置为非键模式
                free(current_key);
                in_key = 0;
            } else {
                // 保存当前键，并检查它是否是已知的键
                current_key = strdup((char *)event.data.scalar.value);
                // 如果键未知，释放它并设置状态变量为非键模式
                if (!is_known_key(current_key)) {
                    log_error("Unknown key in config file");
                    free(current_key);
                    in_key = 0;
                } else {
                    // 否则，也就是如果键是已知的，设置状态变量为键模式
                    in_key = 1;
                }
            }
        }

        // 释放当前YAML事件
        yaml_event_delete(&event);
    }
    // 释放yaml解析器
    yaml_parser_delete(&parser);
    fclose(database_operation_interface_conf);
    // 返回1，表示成功读取配置
    return 1;
}


// 释放配置文件变量
void free_config(struct db_config *cfg) {
    // 释放 db_config 结构体中的各个字段
    free(cfg->server_port);
    free(cfg->database_ip);
    free(cfg->database_port);
    free(cfg->database_privilege_user);
    free(cfg->database_privilege_password);
    free(cfg->log_file);
    free(cfg->user_database_name);
}


// 声明连接池
MYSQL *conn_pool[10];

// 初始化连接池
void initialize_conn_pool(struct db_config *cfg) {
    // 遍历连接池数组，对每个连接进行初始化
    for (int i = 0; i < 10; i++) {
        // 初始化 MySQL 连接
        MYSQL *con = mysql_init(NULL);
        // 如果初始化失败，记录错误并退出
        if (con == NULL) {
            log_error("mysql_init() failed");
            exit(EXIT_FAILURE);
        }
        // 连接到数据库
        if (mysql_real_connect(con, cfg->database_ip, cfg->database_privilege_user,
                               cfg->database_privilege_password, cfg->user_database_name, atoi(cfg->database_port), NULL, 0) == NULL) {
            // 如果连接失败，记录错误，关闭连接并退出
            log_error("mysql_real_connect() failed");
            mysql_close(con);
            exit(EXIT_FAILURE);
        }
        // 将成功初始化的连接保存到连接池数组
        conn_pool[i] = con;
    }
}


// 销毁连接池
void destroy_conn_pool() {
    // 遍历连接池数组，关闭所有数据库连接
    for (int i = 0; i < 10; i++) {
        if (conn_pool[i] != NULL) {
            // 关闭数据库连接
            mysql_close(conn_pool[i]);
        }
    }
}


// 检查数据库连接状态
MYSQL *get_connection_from_pool(struct db_config *cfg, int index) {
    // 从连接池中获取指定索引的连接
    MYSQL *con = conn_pool[index];
    // 使用 mysql_ping() 函数检查连接是否有效
    if (mysql_ping(con) != 0) {
        // 如果连接失效，记录错误并重新初始化该连接
        log_error("Lost connection to database. Reconnecting...");
        reinitialize_conn_pool(cfg, index);
        // 重新获取连接
        con = conn_pool[index];
    }
    return con;
}


// 重新初始化连接
void reinitialize_conn_pool(struct db_config *cfg, int index) {
    // 初始化新的 MySQL 连接
    MYSQL *con = mysql_init(NULL);
    // 如果初始化失败，记录错误并退出
    if (con == NULL) {
        log_error("mysql_init() failed");
        exit(EXIT_FAILURE);
    }
    // 连接到数据库
    if (mysql_real_connect(con, cfg->database_ip, cfg->database_privilege_user,
                           cfg->database_privilege_password, cfg->user_database_name, atoi(cfg->database_port), NULL, 0) == NULL) {
        // 如果连接失败，记录错误，关闭连接并退出
        log_error("mysql_real_connect() failed");
        mysql_close(con);
        exit(EXIT_FAILURE);
    }
    // 将成功初始化的新连接保存到连接池数组的指定索引
    conn_pool[index] = con;
}


// 添加用户的虚拟路径执行函数
void add_user_from_pool_cb(struct evhttp_request *req, void *arg) {
    // 解析 HTTP 请求中的查询字符串并存储在 headers 结构体中
    struct evkeyvalq headers;
    evhttp_parse_query_str(evhttp_uri_get_query(evhttp_request_get_evhttp_uri(req)), &headers);

    // 从 HTTP 请求的 "Authorization" 头中读取 token
    // 这里假设 token 是写死的，因为还没有实现鉴权逻辑
    const char *token = evhttp_find_header(&headers, "Authorization");
    if (token == NULL || strcmp(token, "Bearer YOUR_ACCESS_TOKEN") != 0) {
        // 如果 token 无效或缺失，返回 401 Unauthorized
        evhttp_send_reply(req, 401, "Unauthorized", NULL);
        return;
    }

    // 从 POST 请求的主体中读取 JSON 数据
    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    char data[len + 1];
    evbuffer_remove(buf, data, len);
    data[len] = '\0';

    // 解析 JSON 数据，并进行错误处理
    json_error_t json_err;
    json_t *root = json_loads(data, 0, &json_err);
    if (!root) {
        // 如果 JSON 数据无效，返回 400 Bad Request
        evhttp_send_reply(req, 400, "Bad Request", NULL);
        return;
    }

    // 从 JSON 对象中提取 "user" 和 "password" 字段
    json_t *user_json = json_object_get(root, "user");
    json_t *password_json = json_object_get(root, "password");
    if (!json_is_string(user_json) || !json_is_string(password_json)) {
        // 如果这些字段不是字符串，返回 500 Internal Server Error
        evhttp_send_reply(req, 500, "Internal Server Error", NULL);
        json_decref(root);
        return;
    }

    // 提取用户和密码的实际字符串值
    const char *user = json_string_value(user_json);
    const char *password = json_string_value(password_json);

    // 调用数据库函数以添加新用户
    srand(time(NULL));  // 初始化随机数生成器
    int index = rand() % 10;  // 随机选择连接池中的一个连接
    MYSQL *mysql = get_connection_from_pool(&cfg, index);  // 从连接池中获取该连接
    add_user_from_pool(mysql, user, password);  // 添加用户

    // 发送 HTTP 200 OK 响应
    evhttp_send_reply(req, HTTP_OK, "OK", NULL);
}


// 添加用户的sql数据化输出
void add_user_from_pool(MYSQL *mysql, char *username, char *password) {
    // 初始化 MySQL 语句和绑定变量
    MYSQL_STMT *stmt;
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    
    // 准备 SQL 语句
    stmt = mysql_stmt_init(mysql);
    if (mysql_stmt_prepare(stmt, "INSERT INTO users (username, password) VALUES (?, ?)", -1)) {
        log_error("Failed to prepare statement");
        return;
    }

    // 设置绑定变量
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = username;
    bind[0].buffer_length = strlen(username);
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = password;
    bind[1].buffer_length = strlen(password);
    
    // 绑定参数并检查是否成功
    if (mysql_stmt_bind_param(stmt, bind)) {
        log_error("Failed to bind parameters");
        return;
    }
    
    // 执行 SQL 语句并检查是否成功
    int tmp_result = mysql_stmt_execute(stmt);
    if (tmp_result) {
        log_error("Failed to execute query: %s", mysql_stmt_error(stmt));
        return;
    }
    free(tmp_result)
    
    // 关闭 MySQL 语句
    mysql_stmt_close(stmt);
}



int main() {
    // 初始化 db_config 结构体和 libevent 的基础结构
    struct db_config cfg = {0};
    struct event_base *base = event_base_new();
    struct evhttp *http_server = evhttp_new(base);
    
    // 加载配置文件
    if (!load_config(&cfg)) {
        log_error("Could not load configuration");
        exit(EXIT_FAILURE);
    }
    
    // 初始化日志和数据库连接池
    init_logging(&cfg);
    initialize_conn_pool(&cfg);

    // 设置 HTTP 路由回调
    evhttp_set_cb(http_server, "/add_user_from_pool", add_user_from_pool_cb, NULL);

    // 绑定 HTTP 服务器到指定端口
    if (evhttp_bind_socket(http_server, "0.0.0.0", cfg->server_port) != 0) {
        // 这里需要添加错误处理逻辑
        return 1;
    }

    // 启动事件循环
    event_base_dispatch(base);

    // 释放 libevent 和 libevhttp 资源
    evhttp_free(http_server);
    event_base_free(base);

    // 添加了一个测试用户，测试下函数逻辑，实际应用中不需要这一行
    add_user_from_pool(mysql, "test_user", "test_password");

    // 销毁数据库连接池和释放配置资源
    destroy_conn_pool();
    free_config(&cfg);
    
    return 0;
}
