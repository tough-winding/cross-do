// 整体目的：控制数据库的增删改查操作，均由该服务提供，对外发布socket服务
// 访问方式为 curl -X POST "http://1.1.1.1:19000/add_user_from_pool" -H "Authorization: Bearer YOUR_ACCESS_TOKEN" -H "Content-Type: application/json" -d '{"user":"test_user", "password":"12345"}'
// add_user_from_pool函数的输入参数为user password 两个字符串类型

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>
#include <mysql/mysql.h>
#include <time.h>
#include <log4c.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
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
    NULL
};

// 用于后续检查key是否存在 db_config_keys 这个数组里的函数
int is_known_key(const char *key) {
    for (int i = 0; db_config_keys[i] != NULL; i++) {
        if (strcmp(key, db_config_keys[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// 初始化log4c
void init_logging(struct db_config *cfg) {
    if (log4c_init() == -1 || log4c_load_config(cfg->log_file) == -1) {
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
    FILE *database_operation_interface_conf = fopen("config/config.yaml", "r");
    if (database_operation_interface_conf == NULL) {
        return 0;
    }
    yaml_parser_t parser;
    yaml_event_t event;

    // 初始化yaml解析器
    if (!yaml_parser_initialize(&parser)) {
        fclose(database_operation_interface_conf);
        return 0;
    }

    // 设置输入文件
    yaml_parser_set_input_file(&parser, database_operation_interface_conf);

    int in_key = 0;
    char *current_key = NULL;

    // 开始解析配置文件，将配置文件中的配置项赋值给cfg
    while (1) {
        yaml_parser_parse(&parser, &event);
        
        if (event.type == YAML_STREAM_END_EVENT) {
            break;
        }

        if (event.type == YAML_SCALAR_EVENT) {
            if (in_key) {
                char *value = (char *)event.data.scalar.value;

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
                // 释放当前key
                free(current_key);
                in_key = 0;
            } else {
                // 保存当前key
                current_key = strdup((char *)event.data.scalar.value);
                // 如果 current_key 不是db_config结构体里声明的已知的键，则释放它
                if (!is_known_key(current_key)) {
                    log_error("Unknown key in config file");
                    free(current_key);
                    in_key = 0;
                } else {
                    in_key = 1;
                }
            }
        }

        // 释放当前事件
        yaml_event_delete(&event);
    }
    // 释放yaml解析器
    yaml_parser_delete(&parser);
    fclose(database_operation_interface_conf);
    return 1;
}


// 释放配置文件变量
void free_config(struct db_config *cfg) {
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
    for (int i = 0; i < 10; i++) {
        MYSQL *con = mysql_init(NULL);
        if (con == NULL) {
            log_error("mysql_init() failed");
            exit(EXIT_FAILURE);
        }
        // Connect to the database
        if (mysql_real_connect(con, cfg->database_ip, cfg->database_privilege_user,
                               cfg->database_privilege_password, cfg->user_database_name, atoi(cfg->database_port), NULL, 0) == NULL) {
            log_error("mysql_real_connect() failed");
            mysql_close(con);
            exit(EXIT_FAILURE);
        }
        conn_pool[i] = con;
    }
}

// 销毁连接池
void destroy_conn_pool() {
    for (int i = 0; i < 10; i++) {
        if (conn_pool[i] != NULL) {
            mysql_close(conn_pool[i]);
        }
    }
}

// 检查数据库连接状态
MYSQL *get_connection_from_pool(struct db_config *cfg, int index) {
    MYSQL *con = conn_pool[index];
    if (mysql_ping(con) != 0) {  // 检查连接是否有效
        log_error("Lost connection to database. Reconnecting...");
        reinitialize_conn_pool(cfg, index);
        con = conn_pool[index];
    }
    return con;
}


// 重新初始化连接
void reinitialize_conn_pool(struct db_config *cfg, int index) {
    MYSQL *con = mysql_init(NULL);
    if (con == NULL) {
        log_error("mysql_init() failed");
        exit(EXIT_FAILURE);
    }
    if (mysql_real_connect(con, cfg->database_ip, cfg->database_privilege_user,
                           cfg->database_privilege_password, cfg->user_database_name, atoi(cfg->database_port), NULL, 0) == NULL) {
        log_error("mysql_real_connect() failed");
        mysql_close(con);
        exit(EXIT_FAILURE);
    }
    conn_pool[index] = con;
}

// 添加用户的虚拟路径执行函数
void add_user_from_pool_cb(struct evhttp_request *req, void *arg) {
    struct evkeyvalq headers;
    evhttp_parse_query_str(evhttp_uri_get_query(evhttp_request_get_evhttp_uri(req)), &headers);
    
    // 从 Authorization 头部读取 token 并鉴权，目前由于还没写鉴权逻辑，所以暂且为把token写死
    const char *token = evhttp_find_header(&headers, "Authorization");
    if (token == NULL || strcmp(token, "Bearer YOUR_ACCESS_TOKEN") != 0) {
        evhttp_send_reply(req, 401, "Unauthorized", NULL);
        return;
    }

    // 从 POST 数据中读取 JSON 参数
    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    char data[len + 1];
    evbuffer_remove(buf, data, len);
    data[len] = '\0';

    json_error_t json_err;
    json_t *root = json_loads(data, 0, &json_err);
    if (!root) {
        evhttp_send_reply(req, 400, "Bad Request", NULL);
        return;
    }

    json_t *user_json = json_object_get(root, "user");
    json_t *password_json = json_object_get(root, "password");

    if (!json_is_string(user_json) || !json_is_string(password_json)) {
        evhttp_send_reply(req, 500, "Internal Server Error", NULL);
        json_decref(root);
        return;
    }

    const char *user = json_string_value(user_json);
    const char *password = json_string_value(password_json);

    // 调用数据库函数
    srand(time(NULL));  // 设置随机数种子
    int index = rand() % 10;  // 随机选择一个索引
    MYSQL *mysql = get_connection_from_pool(&cfg, index);  // 从连接池中取出一个连接
    add_user_from_pool(mysql, user, password);

    // 发送HTTP响应
    evhttp_send_reply(req, HTTP_OK, "OK", NULL);
}

// 添加用户的sql数据化输出
void add_user_from_pool(MYSQL *mysql, char *username, char *password) {
    MYSQL_STMT *stmt;
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    
    stmt = mysql_stmt_init(mysql);
    if (mysql_stmt_prepare(stmt, "INSERT INTO users (username, password) VALUES (?, ?)", -1)) {
        log_error("Failed to prepare statement");
        return;
    }

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = username;
    bind[0].buffer_length = strlen(username);
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = password;
    bind[1].buffer_length = strlen(password);
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        log_error("Failed to bind parameters");
        return;
    }
    
    if (mysql_stmt_execute(stmt)) {
        log_error("Failed to execute query");
        return;
    }
    
    mysql_stmt_close(stmt);
}


int main() {
    struct db_config cfg = {0};  //初始化结构体，并暂且将值设为NULL
    struct event_base *base = event_base_new();
    struct evhttp *http_server = evhttp_new(base);

    
    // 加载配置和错误检查
    if (!load_config(&cfg)) {
        log_error("Could not load configuration");
        exit(EXIT_FAILURE);
    }
    
    init_logging(&cfg);
    
    initialize_conn_pool(&cfg);



    // 路径路由
    evhttp_set_cb(http_server, "/add_user_from_pool", add_user_from_pool_cb, NULL);

    // 绑定到 0.0.0.0:server_port
    if (evhttp_bind_socket(http_server, "0.0.0.0", cfg->server_port) != 0) {
        // 这里后面要补错误处理逻辑
        return 1;
    }

    event_base_dispatch(base);

    evhttp_free(http_server);
    event_base_free(base);


    add_user_from_pool(mysql, "test_user", "test_password");


    destroy_conn_pool();
    free_config(&cfg);
    
    return 0;
}
