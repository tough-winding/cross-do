import re
import json

#def extract_fields_from_curl(curl_command):
#    # 提取JSON数据部分
#    json_str  = re.search(r"-d\s+'({.*})'", curl_command).group(1)
#    print(json_str)
#    json_dict = json.loads(json_str)
#    json_list = [{"key": k, "value": v} for k, v in json_dict.items()]
#
#    return(json_list)
def format_json_key(json_key):
    parts = json_key.split('_')
    formatted_key = parts[0].lower() + ''.join(word.capitalize() for word in parts[1:])
    return formatted_key

def format_json_key_with_first_cap(json_key):
    # 将字符串按下划线分割
    parts = json_key.split('_')
    # 将每部分的首字母大写，然后拼接在一起
    formatted_key = ''.join(word.capitalize() for word in parts)
    # 确保整个字符串的第一个字母大写
    formatted_key = formatted_key[0].upper() + formatted_key[1:]
    return formatted_key


def extract_fields_from_curl(curl_command):
    # 提取JSON数据部分
    json_data_match = re.search(r"-d '({.*?})'", curl_command)
    if json_data_match:
        json_data = json.loads(json_data_match.group(1))
        return list(json_data.keys()), json_data
    return []

def generate_sql_code(curl_command, table_name, fields_list):
    # 从CURL命令提取字段信息
    data_fields,data_json = extract_fields_from_curl(curl_command)

    if not data_fields:
        raise ValueError("未能从CURL命令中提取到字段信息")

    if len(data_fields) != len(fields_list):
        print(data_fields)
        print(fields_list)
        raise ValueError("CURL请求中的字段数量与提供的字段映射数量不匹配")

    # 转换表名为合适的格式
    table_name_cap = table_name.capitalize()
    table_name_lower = table_name.lower()

    # 1. struct声明
    struct_decl = f"#1、struct声明\n    MYSQL_STMT *stmt_insert_{table_name_lower};\n"

    # 2. 预处理语句
    placeholders = ', '.join(['?' for _ in fields_list])
    insert_sql = f"INSERT INTO {table_name} ({', '.join(fields_list)}) VALUES ({placeholders});"
    prepare_stmt = f"""#2、预处理语句
    DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_{table_name_lower} = mysql_stmt_init(DBOP_VAR_InitializeMySQLConnection_connect->mysql);
    const char *insert_{table_name_lower}_sql = "{insert_sql}";
    if (mysql_stmt_prepare(DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_{table_name_lower}, insert_{table_name_lower}_sql, strlen(insert_{table_name_lower}_sql))) {{
        dzlog_error("Failed to prepare insert statement: %s", mysql_stmt_error(DBOP_VAR_InitializeMySQLConnection_connect->stmt_insert_{table_name_lower}));
        exit(EXIT_FAILURE);
    }}
"""

    # 3. 销毁连接池
    destroy_conn_pool = f"""#3、销毁连接池：
        if (DBOP_GLV_mysqlConnectPool[i]->stmt_insert_{table_name_lower} != NULL) {{
            mysql_stmt_close(DBOP_GLV_mysqlConnectPool[i]->stmt_insert_{table_name_lower});
        }}
"""

    # 4. 重新初始化mysql连接时，先销毁连接池
    reinit_conn_pool = f"""#4、重新初始化mysql连接时，先销毁连接池：
    if (DBOP_VAR_ReinitializeConnPool_connect->stmt_insert_{table_name_lower} != NULL) {{
        mysql_stmt_close(DBOP_VAR_ReinitializeConnPool_connect->stmt_insert_{table_name_lower});
    }}
"""

    # 5. 执行SQL的函数主体
    bind_params = ""
    variables_declaration = ""
    i = 0
    for key, value in data_json.items():
        if isinstance(value, str):
            bind_params += f"""
    DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams[{i}].buffer_type = MYSQL_TYPE_STRING;
    DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams[{i}].buffer = (char *)DBOP_VAR_ExecuteCreate{table_name_cap}_{format_json_key(key)};
    DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams[{i}].buffer_length = strlen(DBOP_VAR_ExecuteCreate{table_name_cap}_{format_json_key(key)});
"""
            variables_declaration += f"const char *DBOP_VAR_ExecuteCreate{table_name_cap}_{format_json_key(key)}, "
        if isinstance(value, int):
            bind_params += f"""
    DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams[{i}].buffer_type = MYSQL_TYPE_TINY;
    DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams[{i}].buffer = &DBOP_VAR_ExecuteCreate{table_name_cap}_{format_json_key(key)};
    DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams[{i}].buffer_length = sizeof(DBOP_VAR_ExecuteCreate{table_name_cap}_{format_json_key(key)});
"""
            variables_declaration += f"int DBOP_VAR_ExecuteCreate{table_name_cap}_{format_json_key(key)}, "
        i = i + 1
    variables_declaration = variables_declaration.rstrip(", ")


    execute_function = f"""#5、执行sql的函数主体：
int DBOP_FUN_ExecuteCreate{table_name_cap}(DB_CONNECTION *DBOP_VAR_ExecuteCreate{table_name_cap}_connect, {variables_declaration}, const char *DBOP_VAR_ExecuteCreate{table_name_cap}_requestId) {{
    dzlog_info("[req: %s] DBOP_FUN_ExecuteCreate{table_name_cap} is starting", DBOP_VAR_ExecuteCreate{table_name_cap}_requestId);

    MYSQL_BIND DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams[{len(fields_list)}];
    memset(DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams, 0, sizeof(DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams));

    {bind_params}

    if (mysql_stmt_bind_param(DBOP_VAR_ExecuteCreate{table_name_cap}_connect->stmt_insert_{table_name_lower}, DBOP_VAR_ExecuteCreate{table_name_cap}_bindParams)) {{
        dzlog_error("[req: %s] Failed to bind insert param: %s", DBOP_VAR_ExecuteCreate{table_name_cap}_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreate{table_name_cap}_connect->stmt_insert_{table_name_lower}));
        return -1;
    }}

    if (mysql_stmt_execute(DBOP_VAR_ExecuteCreate{table_name_cap}_connect->stmt_insert_{table_name_lower})) {{
        dzlog_error("[req: %s] Failed to execute insert statement: %s", DBOP_VAR_ExecuteCreate{table_name_cap}_requestId, mysql_stmt_error(DBOP_VAR_ExecuteCreate{table_name_cap}_connect->stmt_insert_{table_name_lower}));
        return -1;
    }}

    dzlog_info("[req: %s] Successfully created {table_name}", DBOP_VAR_ExecuteCreate{table_name_cap}_requestId);
    return 0;
}}
"""

    # 6. 创建项目的API接口
    json_extractions = "\n    ".join([f'json_t *DBOP_VAR_ApiCreate{table_name_cap}_dataJson{format_json_key_with_first_cap(key)} = json_object_get(DBOP_VAR_ApiCreate{table_name_cap}_dataJsonAll, "{key}");' for key, value in data_json.items()])
    # 生成类型检查代码
    type_checks = ""
    for key, value in data_json.items():
        if isinstance(value, str):
            type_checks += f'!json_is_string(DBOP_VAR_ApiCreate{table_name_cap}_dataJson{format_json_key_with_first_cap(key)}) ||\n        '
        elif isinstance(value, int):
            type_checks += f'!json_is_integer(DBOP_VAR_ApiCreate{table_name_cap}_dataJson{format_json_key_with_first_cap(key)}) ||\n        '
    # 去掉最后一个 '||' 
    type_checks = type_checks.rstrip(' ||\n        ')

    define_type = ""
    for key, value in data_json.items():
        if isinstance(value, str):
            define_type += f'const char *DBOP_VAR_ApiCreate{table_name_cap}_{format_json_key(key)} = json_string_value(DBOP_VAR_ApiCreate{table_name_cap}_dataJson{format_json_key_with_first_cap(key)});\n    '
        elif isinstance(value, int):
            define_type += f'int DBOP_VAR_ApiCreate{table_name_cap}_{format_json_key(key)} = json_integer_value(DBOP_VAR_ApiCreate{table_name_cap}_dataJson{format_json_key_with_first_cap(key)});\n    '

    api_function = f"""
void DBOP_FUN_ApiCreate{table_name_cap}(struct evhttp_request *DBOP_VAR_ApiCreate{table_name_cap}_request, void *DBOP_VAR_ApiCreate{table_name_cap}_voidCfg) {{
    AppConfig *DBOP_VAR_ApiCreate{table_name_cap}_cfg = (AppConfig *)DBOP_VAR_ApiCreate{table_name_cap}_voidCfg;
    const char *DBOP_VAR_ApiCreate{table_name_cap}_requestId = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiCreate{table_name_cap}_request), "X-Request-ID");
    char uuid_str[37];
    if (!DBOP_VAR_ApiCreate{table_name_cap}_requestId) {{
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, uuid_str);
        DBOP_VAR_ApiCreate{table_name_cap}_requestId = uuid_str;
    }}
    dzlog_info("[req: %s] Processing API request to ApiCreate{table_name_cap}.", DBOP_VAR_ApiCreate{table_name_cap}_requestId);

    struct evkeyvalq DBOP_VAR_ApiCreate{table_name_cap}_headers;
    evhttp_parse_query_str(evhttp_uri_get_query(evhttp_request_get_evhttp_uri(DBOP_VAR_ApiCreate{table_name_cap}_request)), &DBOP_VAR_ApiCreate{table_name_cap}_headers);

    const char *DBOP_VAR_ApiCreate{table_name_cap}_serviceName = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiCreate{table_name_cap}_request), "ServiceName");
    const char *DBOP_VAR_ApiCreate{table_name_cap}_token = evhttp_find_header(evhttp_request_get_input_headers(DBOP_VAR_ApiCreate{table_name_cap}_request), "Authorization");
    if (DBOP_FUN_AuthenticateRequest(DBOP_VAR_ApiCreate{table_name_cap}_cfg, DBOP_VAR_ApiCreate{table_name_cap}_serviceName, DBOP_VAR_ApiCreate{table_name_cap}_token)) {{
        dzlog_info("[req: %s] Request authentication approval.", DBOP_VAR_ApiCreate{table_name_cap}_requestId);
    }} else {{
        dzlog_warn("[req: %s] Unauthorized access attempt.", DBOP_VAR_ApiCreate{table_name_cap}_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreate{table_name_cap}_request, 401, "Unauthorized", NULL);
        return;
    }}

    struct evbuffer *DBOP_VAR_ApiCreate{table_name_cap}_inputBuffer = evhttp_request_get_input_buffer(DBOP_VAR_ApiCreate{table_name_cap}_request);
    size_t DBOP_VAR_ApiCreate{table_name_cap}_bufferLen = evbuffer_get_length(DBOP_VAR_ApiCreate{table_name_cap}_inputBuffer);
    char DBOP_VAR_ApiCreate{table_name_cap}_postData[DBOP_VAR_ApiCreate{table_name_cap}_bufferLen + 1];
    evbuffer_remove(DBOP_VAR_ApiCreate{table_name_cap}_inputBuffer, DBOP_VAR_ApiCreate{table_name_cap}_postData, DBOP_VAR_ApiCreate{table_name_cap}_bufferLen);
    DBOP_VAR_ApiCreate{table_name_cap}_postData[DBOP_VAR_ApiCreate{table_name_cap}_bufferLen] = '\\0';

    json_error_t DBOP_VAR_ApiCreate{table_name_cap}_dataJsonError;
    json_t *DBOP_VAR_ApiCreate{table_name_cap}_dataJsonAll = json_loads(DBOP_VAR_ApiCreate{table_name_cap}_postData, 0, &DBOP_VAR_ApiCreate{table_name_cap}_dataJsonError);
    dzlog_info("[req: %s] Parsing POST data.", DBOP_VAR_ApiCreate{table_name_cap}_requestId);
    if (!DBOP_VAR_ApiCreate{table_name_cap}_dataJsonAll) {{
        dzlog_error("[req: %s] Failed to parse JSON from request body.", DBOP_VAR_ApiCreate{table_name_cap}_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreate{table_name_cap}_request, 400, "Bad Request", NULL);
        return;
    }}

    {json_extractions}

    if ({type_checks}) {{
        dzlog_error("[req: %s] Invalid JSON data received.", DBOP_VAR_ApiCreate{table_name_cap}_requestId);
        evhttp_send_reply(DBOP_VAR_ApiCreate{table_name_cap}_request, 400, "Bad Request", NULL);
        json_decref(DBOP_VAR_ApiCreate{table_name_cap}_dataJsonAll);
        return;
    }}

    dzlog_info("[req: %s] Executing database operation for ApiCreate{table_name_cap}.", DBOP_VAR_ApiCreate{table_name_cap}_requestId);

    {define_type}

    int index = rand() % 10;
    DB_CONNECTION *DBOP_VAR_ApiCreate{table_name_cap}_mysqlConnect = DBOP_FUN_GetConnectFromPool(DBOP_VAR_ApiCreate{table_name_cap}_cfg, index);

    int result = DBOP_FUN_ExecuteCreate{table_name_cap}(DBOP_VAR_ApiCreate{table_name_cap}_mysqlConnect, {", ".join([f"DBOP_VAR_ApiCreate{table_name_cap}_{format_json_key(key)}" for key, value in data_json.items()])}, DBOP_VAR_ApiCreate{table_name_cap}_requestId);


    if (result == 0) {{
        evhttp_send_reply(DBOP_VAR_ApiCreate{table_name_cap}_request, HTTP_OK, "OK", NULL);
        dzlog_info("[req: %s] Successfully created {table_name}, returning 200", DBOP_VAR_ApiCreate{table_name_cap}_requestId);
    }} else {{
        evhttp_send_reply(DBOP_VAR_ApiCreate{table_name_cap}_request, HTTP_INTERNAL, "Internal Server Error", NULL);
        dzlog_error("[req: %s] Failed to create {table_name}, returning 500", DBOP_VAR_ApiCreate{table_name_cap}_requestId);
    }}

    json_decref(DBOP_VAR_ApiCreate{table_name_cap}_dataJsonAll);
}}
"""

    # 7. 添加路由路径
    route_path = f'#6、添加路由路径：\n    evhttp_set_cb(DBOP_VAR_Main_httpServer, "/create_{table_name_lower}", DBOP_FUN_ApiCreate{table_name_cap}, &DBOP_VAR_Main_cfg);\n'

    # 提取JSON字段的代码
    json_extractions = "\n".join([f'json_t *DBOP_VAR_ApiCreate{table_name_cap}_dataJson{format_json_key_with_first_cap(key)}) = json_object_get(DBOP_VAR_ApiCreate{table_name_cap}_dataJsonAll, "{key}");' for key, value in data_json.items()])

    # 合并所有代码段
    full_code = struct_decl + prepare_stmt + destroy_conn_pool + reinit_conn_pool + execute_function + api_function + route_path
    full_code = full_code.replace("{json_extractions}", json_extractions)
    return full_code

# 示例调用
#curl_command = "curl -X POST \"http://服务ip:服务端口/create_porject\" -H \"ServiceName: service服务名\" -H \"Authorization: service服务token\" -H \"Content-Type: application/json\" -d '{\"project_id\":\"project ID\", \"user_name\":\"用户名\", \"user_age\":用户年龄, \"real_name\":\"用户真实姓名\"}'"
curl_command = """curl -X POST "http://192.168.1.10:1900/create_porject" -H "ServiceName: IAM_SERVICE" -H "Authorization: WuWVKPN3EaPkLStZP8DxLKLcaANN6NVc" -H "Content-Type: application/json" -d '{"project_id":"d7aa0e59-9be6-4c76-9bf1-ee937791b8a8", "user_name":"两茫茫", "user_age":32, "real_name":"李通"}'"""

fields_map = {
    "insert": {
        "table_name": "project",
        "fields": ["project_id", "user_name", "user_age", "real_name"]
    }
}
table_name = fields_map['insert']['table_name']


print(generate_sql_code(curl_command, table_name, fields_map['insert']['fields']))