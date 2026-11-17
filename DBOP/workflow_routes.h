#ifndef CROSSDO_WORKFLOW_ROUTES_H
#define CROSSDO_WORKFLOW_ROUTES_H
#include "workflow_common.h"
#include "workflow_support.h"
#include "workflow_files.h"
#include "workflow_history.h"
#include "workflow_accounts.h"
#include "workflow_projects.h"
#include "workflow_security.h"
#include "workflow_operations.h"
#include "workflow_finance.h"

static json_t *wf_account_login(WF_Context *ctx, json_t *body) {
    json_t *user = wa_actor(ctx, body, false);
    if (!user) return NULL;
    int status = wa_integer(user, "user_status");
    if (!json_is_integer(json_object_get(user, "user_status")) || status < 0 || status >= 8) {
        json_decref(user); return wf_fail(ctx, 403, "Account cannot log in");
    }
    const char *params[] = {wa_string(user, "user_id")};
    if (!wf_exec(ctx, "UPDATE user SET login_time=UTC_TIMESTAMP() WHERE user_id=?", 1, params)) {
        json_decref(user); return NULL;
    }
    char now[20]; wf_now(now);
    json_object_set_new(user, "login_time", json_string(now));
    json_t *result = wa_public_account(user); json_decref(user); return result;
}

typedef json_t *(*WF_Action)(WF_Context *, json_t *);
typedef struct { const char *path; WF_Action action; } WF_Route;
static const WF_Route wf_routes[] = {
    {"/workflow/finance/order_create", fin_order_create},
    {"/workflow/finance/order_get", fin_order_get},
    {"/workflow/finance/orders_list", fin_orders_list},
    {"/workflow/finance/order_checkout", fin_order_checkout},
    {"/workflow/finance/refund_create", fin_refund_create},
    {"/workflow/finance/refund_get", fin_refund_get},
    {"/workflow/finance/disbursement_create", fin_disbursement_create},
    {"/workflow/finance/disbursement_get", fin_disbursement_get},
    {"/workflow/finance/disbursements_list", fin_disbursements_list},
    {"/workflow/finance/disbursement_review", fin_disbursement_review},
    {"/workflow/finance/complete", fin_complete},
    {"/workflow/finance/funds", fin_funds},
    {"/workflow/finance/reconciliation", fin_reconciliation},
    {"/workflow/files/register", wf_file_register},
    {"/workflow/files/access", wf_file_access},
    {"/workflow/security/version", wf_security_version},
    {"/workflow/security/session", wf_security_session},
    {"/workflow/security/credentials", wf_security_credentials},
    {"/workflow/security/reset-password", wf_security_reset_password},
    {"/workflow/security/change-phone", wf_security_change_phone},
    {"/workflow/ops/overview", wf_ops_overview},
    {"/workflow/ops/users", wf_ops_users},
    {"/workflow/ops/certification", wf_ops_certification},
    {"/workflow/ops/account-state", wf_ops_account_state},
    {"/workflow/ops/tasks", wf_ops_tasks},
    {"/workflow/ops/reassign", wf_ops_reassign},
    {"/workflow/ops/notifications", wf_ops_notifications},
    {"/workflow/ops/notifications-unread", wf_ops_unread},
    {"/workflow/ops/notification-read", wf_ops_notification_read},
    {"/workflow/ops/notifications-read-all", wf_ops_notifications_read_all},
    {"/workflow/ops/case-create", wf_ops_case_create},
    {"/workflow/ops/cases", wf_ops_cases},
    {"/workflow/ops/admin-cases", wf_ops_admin_cases},
    {"/workflow/ops/case-get", wf_ops_case_get},
    {"/workflow/ops/case-resolve", wf_ops_case_resolve},
    {"/workflow/ops/closure-request", wf_ops_closure_request},
    {"/workflow/ops/closure-get", wf_ops_closure_get},
    {"/workflow/ops/closures", wf_ops_closures},
    {"/workflow/ops/closure-review", wf_ops_closure_review},
    {"/workflow/register", wf_register},
    {"/workflow/credentials", wf_credentials},
    {"/workflow/account/get", wf_account_get},
    {"/workflow/account/login", wf_account_login},
    {"/workflow/application/submit", wf_application_submit},
    {"/workflow/application/list", wf_application_list},
    {"/workflow/application/get", wf_application_get},
    {"/workflow/application/claim", wf_application_claim},
    {"/workflow/application/review", wf_application_review},
    {"/workflow/project/create", wf_project_create},
    {"/workflow/project/submit", wf_project_submit},
    {"/workflow/project/get", wf_project_get},
    {"/workflow/project/list", wf_project_list},
    {"/workflow/project/tasks", wf_project_tasks},
    {"/workflow/project/claim", wf_project_claim},
    {"/workflow/project/review", wf_project_review},
    {"/workflow/project/pathography", wf_project_pathography}
};

static void wf_response(struct evhttp_request *request, int status, json_t *body) {
    struct evbuffer *buffer = evbuffer_new();
    char *encoded = body ? json_dumps(body, JSON_COMPACT) : NULL;
    if (!buffer || !encoded) {
        if (buffer) evbuffer_free(buffer);
        free(encoded); evhttp_send_error(request, 500, "Internal Server Error"); return;
    }
    evhttp_add_header(evhttp_request_get_output_headers(request), "Content-Type", "application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(request), "Cache-Control", "no-store");
    evbuffer_add(buffer, encoded, strlen(encoded));
    evhttp_send_reply(request, status, status < 400 ? "OK" : "Request failed", buffer);
    free(encoded); evbuffer_free(buffer);
}

static void wf_api(struct evhttp_request *request, void *config) {
    AppConfig *cfg = config;
    WF_Context ctx = { .db = NULL, .status = 200, .error = NULL, .changed = 0 };
    json_t *body = NULL, *result = NULL;
    struct evhttp_uri *uri = NULL;
    if (evhttp_request_get_command(request) != EVHTTP_REQ_POST) {
        evhttp_add_header(evhttp_request_get_output_headers(request), "Allow", "POST");
        wf_fail(&ctx, 405, "POST required"); goto reply;
    }
    const char *service = evhttp_find_header(evhttp_request_get_input_headers(request), "ServiceName");
    const char *token = evhttp_find_header(evhttp_request_get_input_headers(request), "Authorization");
    if (!service || !*service || !token || !*token || !DBOP_FUN_AuthenticateRequest(cfg, service, token)) {
        wf_fail(&ctx, 401, "Service authentication required"); goto reply;
    }
    struct evbuffer *input = evhttp_request_get_input_buffer(request);
    size_t size = evbuffer_get_length(input);
    if (!size || size > 1024 * 1024) { wf_fail(&ctx, 400, "JSON body must be between 1 byte and 1 MiB"); goto reply; }
    const unsigned char *raw = evbuffer_pullup(input, -1);
    body = raw ? json_loadb((const char *)raw, size, JSON_REJECT_DUPLICATES, NULL) : NULL;
    if (!json_is_object(body)) { wf_fail(&ctx, 400, "JSON object required"); goto reply; }
    uri = evhttp_uri_parse(evhttp_request_get_uri(request));
    const char *path = uri ? evhttp_uri_get_path(uri) : NULL;
    WF_Action action = NULL;
    for (size_t i = 0; path && i < sizeof(wf_routes) / sizeof(wf_routes[0]); ++i)
        if (!strcmp(path, wf_routes[i].path)) { action = wf_routes[i].action; break; }
    if (!action) { wf_fail(&ctx, 404, "Unknown workflow action"); goto reply; }
    DB_CONNECTION *connection = DBOP_FUN_GetDatabaseConnection(cfg);
    ctx.db = connection ? connection->mysql : NULL;
    if (!ctx.db || mysql_query(ctx.db, "SET time_zone='+00:00'") || mysql_query(ctx.db, "START TRANSACTION")) {
        wf_fail(&ctx, 503, "Database unavailable"); goto reply;
    }
    if (json_object_get(body, "actor_id")) {
        const char *actor_id = wf_actor_id(&ctx, body);
        json_t *actor = actor_id ? wf_user(&ctx, actor_id, false) : NULL;
        if (!actor || !wf_active_account(actor)) {
            if (actor) wf_fail(&ctx, 403, "Account is frozen");
            json_decref(actor); mysql_rollback(ctx.db); goto reply;
        }
        json_decref(actor);
    }
    result = action(&ctx, body);
    if (result && ctx.status < 400) {
        if (mysql_commit(ctx.db)) {
            mysql_rollback(ctx.db); json_decref(result); result = NULL;
            wf_fail(&ctx, 500, "Unable to commit operation");
        }
    } else mysql_rollback(ctx.db);
reply:
    if (!result) {
        if (ctx.status < 400) wf_fail(&ctx, 500, "Operation failed");
        result = json_pack("{s:i,s:s}", "status", ctx.status, "message", ctx.error ? ctx.error : "Operation failed");
    }
    wf_response(request, ctx.status, result);
    json_decref(result); json_decref(body);
    if (uri) evhttp_uri_free(uri);
}

static void wf_register_routes(struct evhttp *server, AppConfig *cfg) {
    for (size_t i = 0; i < sizeof(wf_routes) / sizeof(wf_routes[0]); ++i)
        evhttp_set_cb(server, wf_routes[i].path, wf_api, cfg);
}
#endif
