#ifndef CROSSDO_WORKFLOW_COMMON_H
#define CROSSDO_WORKFLOW_COMMON_H

/* Shared, request-scoped prepared statements. Included after the legacy DBOP helpers. */
#include <stdint.h>
#include <ctype.h>
#include <limits.h>

typedef struct {
    MYSQL *db;
    int status;
    const char *error;
    my_ulonglong changed;
} WF_Context;

static json_t *wf_fail(WF_Context *ctx, int status, const char *message) {
    ctx->status = status;
    ctx->error = message;
    return NULL;
}

static void wf_database_error(WF_Context *ctx, unsigned int code) {
    if (code == 1062) wf_fail(ctx, 409, "Record already exists");
    /* MariaDB's snapshot isolation can reject a current read with ER_CHECKREAD
     * after a concurrent commit. Treat it like a serialization conflict. */
    else if (code == 1020 || code == 1205 || code == 1213) wf_fail(ctx, 409, "Concurrent change; retry the request");
    else if (code == 1451 || code == 1452 || code == 3819 || code == 4025)
        wf_fail(ctx, 409, "Record violates a relationship or state constraint");
    else wf_fail(ctx, 500, "Database operation failed");
}

static MYSQL_STMT *wf_prepare(WF_Context *ctx, const char *sql, size_t count,
                              const char *const *params) {
    MYSQL_STMT *stmt = mysql_stmt_init(ctx->db);
    if (!stmt) { wf_fail(ctx, 500, "Database operation failed"); return NULL; }
    MYSQL_BIND *bindings = count ? calloc(count, sizeof(MYSQL_BIND)) : NULL;
    if ((count && !bindings) || mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        wf_database_error(ctx, mysql_stmt_errno(stmt));
        free(bindings); mysql_stmt_close(stmt); return NULL;
    }
    if (mysql_stmt_param_count(stmt) != count) {
        wf_fail(ctx, 500, "Invalid database statement");
        free(bindings); mysql_stmt_close(stmt); return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        bindings[i].buffer_type = params[i] ? MYSQL_TYPE_STRING : MYSQL_TYPE_NULL;
        bindings[i].buffer = (void *)params[i];
        bindings[i].buffer_length = params[i] ? strlen(params[i]) : 0;
    }
    if ((count && mysql_stmt_bind_param(stmt, bindings)) || mysql_stmt_execute(stmt)) {
        wf_database_error(ctx, mysql_stmt_errno(stmt));
        free(bindings); mysql_stmt_close(stmt); return NULL;
    }
    free(bindings);
    return stmt;
}

static bool wf_exec(WF_Context *ctx, const char *sql, size_t count, const char *const *params) {
    MYSQL_STMT *stmt = wf_prepare(ctx, sql, count, params);
    if (!stmt) return false;
    ctx->changed = mysql_stmt_affected_rows(stmt);
    mysql_stmt_close(stmt);
    return true;
}

static my_ulonglong wf_changed(WF_Context *ctx) { return ctx->changed; }

static bool wf_integer_type(enum enum_field_types type) {
    return type == MYSQL_TYPE_TINY || type == MYSQL_TYPE_SHORT || type == MYSQL_TYPE_LONG ||
           type == MYSQL_TYPE_INT24 || type == MYSQL_TYPE_LONGLONG || type == MYSQL_TYPE_YEAR;
}

static json_t *wf_query(WF_Context *ctx, const char *sql, size_t count, const char *const *params) {
    MYSQL_STMT *stmt = wf_prepare(ctx, sql, count, params);
    if (!stmt) return NULL;
    MYSQL_RES *meta = mysql_stmt_result_metadata(stmt);
    unsigned int columns = meta ? mysql_num_fields(meta) : 0;
    MYSQL_BIND *bindings = calloc(columns, sizeof(MYSQL_BIND));
    unsigned long *lengths = calloc(columns, sizeof(unsigned long));
    /* Use the connector's native null/error flag type (my_bool on MariaDB). */
    __typeof__(*(((MYSQL_BIND *)0)->is_null)) *nulls = calloc(columns, sizeof(*nulls));
    __typeof__(*(((MYSQL_BIND *)0)->error)) *errors = calloc(columns, sizeof(*errors));
    char **buffers = calloc(columns, sizeof(char *));
    json_t *rows = json_array();
    bool failed = !meta || !bindings || !lengths || !nulls || !errors || !buffers || !rows;
    MYSQL_FIELD *fields = meta ? mysql_fetch_fields(meta) : NULL;
    if (!failed) {
        for (unsigned int i = 0; i < columns; ++i) {
            /* First fetch only lengths; fetch_column then allocates exact-sized buffers. */
            bindings[i].buffer_type = MYSQL_TYPE_STRING;
            bindings[i].length = &lengths[i];
            bindings[i].is_null = &nulls[i];
            bindings[i].error = &errors[i];
        }
        failed = mysql_stmt_bind_result(stmt, bindings) || mysql_stmt_store_result(stmt);
    }
    int fetched = MYSQL_NO_DATA;
    while (!failed && (fetched = mysql_stmt_fetch(stmt)) != MYSQL_NO_DATA) {
        if (fetched != 0 && fetched != MYSQL_DATA_TRUNCATED) { failed = true; break; }
        json_t *row = json_object();
        if (!row) { failed = true; break; }
        for (unsigned int i = 0; i < columns && !failed; ++i) {
            json_t *value = NULL;
            if (nulls[i]) value = json_null();
            else {
                if (lengths[i] > 4 * 1024 * 1024) { failed = true; break; }
                buffers[i] = malloc(lengths[i] + 1);
                if (!buffers[i]) { failed = true; break; }
                MYSQL_BIND column = {0};
                column.buffer_type = MYSQL_TYPE_STRING;
                column.buffer = buffers[i];
                column.buffer_length = lengths[i];
                if (mysql_stmt_fetch_column(stmt, &column, i, 0)) { failed = true; break; }
                buffers[i][lengths[i]] = '\0';
                value = wf_integer_type(fields[i].type) ? json_integer(strtoll(buffers[i], NULL, 10))
                                                        : json_stringn(buffers[i], lengths[i]);
                free(buffers[i]); buffers[i] = NULL;
            }
            if (!value || json_object_set_new(row, fields[i].name, value)) failed = true;
        }
        if (!failed) json_array_append_new(rows, row); else json_decref(row);
    }
    if (buffers) for (unsigned int i = 0; i < columns; ++i) free(buffers[i]);
    free(buffers); free(bindings); free(lengths); free(nulls); free(errors);
    if (meta) mysql_free_result(meta);
    if (failed) {
        wf_database_error(ctx, mysql_stmt_errno(stmt));
        json_decref(rows); rows = NULL;
    }
    mysql_stmt_free_result(stmt); mysql_stmt_close(stmt);
    return rows;
}

static json_t *wf_one(WF_Context *ctx, const char *sql, size_t count, const char *const *params) {
    json_t *rows = wf_query(ctx, sql, count, params);
    if (!rows) return NULL;
    json_t *row = json_array_size(rows) ? json_incref(json_array_get(rows, 0)) : NULL;
    json_decref(rows);
    return row ? row : wf_fail(ctx, 404, "Record not found");
}

static const char *wf_text(WF_Context *ctx, json_t *body, const char *key, size_t max_bytes) {
    json_t *value = json_object_get(body, key);
    const char *text = json_string_value(value);
    if (!text || !*text || json_string_length(value) > max_bytes ||
        strlen(text) != json_string_length(value)) {
        wf_fail(ctx, 400, "Missing or invalid text field"); return NULL;
    }
    bool nonspace = false;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) if (!isspace(*p)) nonspace = true;
    if (!nonspace) { wf_fail(ctx, 400, "Text must not be blank"); return NULL; }
    return text;
}

static bool wf_uuid(const char *text) {
    uuid_t id;
    return text && strlen(text) == 36 && uuid_parse(text, id) == 0;
}

static bool wf_reference(const char *text) {
    return text && ((!strncmp(text, "/api/files/", 11) && wf_uuid(text + 11)) ||
                    !strncmp(text, "https://", 8) || !strncmp(text, "http://", 7));
}

static bool wf_active_account(json_t *user) {
    const char *state = json_string_value(json_object_get(user, "account_state"));
    return state && !strcmp(state, "active");
}

static const char *wf_actor_id(WF_Context *ctx, json_t *body) {
    const char *id = wf_text(ctx, body, "actor_id", 36);
    if (id && !wf_uuid(id)) { wf_fail(ctx, 400, "Invalid actor ID"); return NULL; }
    return id;
}

static json_t *wf_user(WF_Context *ctx, const char *id, bool lock) {
    const char *params[] = {id};
    /* Credentials have their own narrow lookup. Identity numbers and unrelated
     * private columns are not needed to authorize or display workflow accounts. */
    return wf_one(ctx, lock
        ? "SELECT user_id,user_permission,user_name,avatar,real_name,personal_note,"
          "real_name_authentication,user_status,existing_projects,project_id,phone_number,"
          "user_age,registration_date,login_time,total_donation_amount,account_state,session_version,phone_verified FROM `user` WHERE user_id=? FOR UPDATE"
        : "SELECT user_id,user_permission,user_name,avatar,real_name,personal_note,"
          "real_name_authentication,user_status,existing_projects,project_id,phone_number,"
          "user_age,registration_date,login_time,total_donation_amount,account_state,session_version,phone_verified FROM `user` WHERE user_id=?", 1, params);
}

static bool wf_is_volunteer(json_t *user) {
    int role = json_integer_value(json_object_get(user, "user_permission"));
    return wf_active_account(user) && (role == 5 || role == 7) &&
           json_is_integer(json_object_get(user, "user_status")) &&
           json_integer_value(json_object_get(user, "user_status")) == 0 &&
           json_integer_value(json_object_get(user, "real_name_authentication")) == 1;
}

static bool wf_is_patient(json_t *user) {
    return wf_active_account(user) && json_integer_value(json_object_get(user, "user_permission")) == 3 &&
           json_is_integer(json_object_get(user, "user_status")) &&
           json_integer_value(json_object_get(user, "user_status")) == 0 &&
           json_integer_value(json_object_get(user, "real_name_authentication")) == 1;
}

static json_t *wf_json_array(WF_Context *ctx, const char *stored) {
    if (!stored || !*stored) return json_array();
    json_t *value = json_loads(stored, 0, NULL);
    if (!json_is_array(value)) {
        json_decref(value); return wf_fail(ctx, 500, "Invalid stored history");
    }
    return value;
}

static char *wf_dump(WF_Context *ctx, json_t *value) {
    char *text = json_dumps(value, JSON_COMPACT);
    if (!text) wf_fail(ctx, 500, "Unable to serialize record");
    else if (strlen(text) > 65535) { free(text); wf_fail(ctx, 400, "Record exceeds storage limit"); return NULL; }
    return text;
}

static void wf_new_id(char output[37]) { uuid_t id; uuid_generate(id); uuid_unparse_lower(id, output); }
static void wf_now(char output[20]) {
    time_t now = time(NULL); struct tm utc;
    gmtime_r(&now, &utc); strftime(output, 20, "%Y-%m-%d %H:%M:%S", &utc);
}

static bool wf_money(const char *amount) {
    if (!amount || !*amount) return false;
    const char *p = amount; size_t digits = 0; bool positive = false;
    while (*p >= '0' && *p <= '9') { if (*p != '0') positive = true; ++p; ++digits; }
    if (digits < 1 || digits > 8 || (digits > 1 && amount[0] == '0')) return false;
    if (*p == '.') {
        size_t decimals = 0; ++p;
        while (*p >= '0' && *p <= '9') { if (*p != '0') positive = true; ++p; ++decimals; }
        if (decimals < 1 || decimals > 2) return false;
    }
    return !*p && positive;
}

#endif
