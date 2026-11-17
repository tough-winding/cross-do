#ifndef CROSSDO_WORKFLOW_SUPPORT_H
#define CROSSDO_WORKFLOW_SUPPORT_H

/* Shared support functions, included immediately after workflow_common.h. */
static bool wf_notify(WF_Context *ctx, const char *user_id, const char *type,
                      const char *title, const char *body, const char *link) {
    if (!wf_uuid(user_id)) { wf_fail(ctx, 500, "Invalid notification recipient"); return false; }
    char id[37]; wf_new_id(id);
    const char *params[] = {id, user_id, type, title, body, link ? link : "/"};
    return wf_exec(ctx, "INSERT INTO notification(id,user_id,type,title,body,link,created_at) "
        "VALUES(?,?,?,?,?,?,UTC_TIMESTAMP())", 6, params);
}

static bool wf_admin_user(json_t *user) {
    const char *state = json_string_value(json_object_get(user, "account_state"));
    return user && json_integer_value(json_object_get(user, "user_permission")) == 1 &&
        json_is_integer(json_object_get(user, "user_status")) &&
        json_integer_value(json_object_get(user, "user_status")) == 0 &&
        json_integer_value(json_object_get(user, "real_name_authentication")) == 1 &&
        state && !strcmp(state, "active");
}

static json_t *wf_require_admin(WF_Context *ctx, json_t *body) {
    const char *id = wf_actor_id(ctx, body);
    if (!id) return NULL;
    json_t *actor = wf_user(ctx, id, false);
    if (!actor) return NULL;
    if (!wf_admin_user(actor)) {
        json_decref(actor); return wf_fail(ctx, 403, "Active administrator account required");
    }
    return actor;
}

static bool wf_admin_audit(WF_Context *ctx, const char *actor, const char *action,
                           const char *kind, const char *target, const char *note) {
    char id[37]; wf_new_id(id);
    const char *params[] = {id, actor, action, kind, target, note};
    return wf_exec(ctx, "INSERT INTO admin_audit(id,actor_id,action,target_type,target_id,note,created_at) "
        "VALUES(?,?,?,?,?,?,UTC_TIMESTAMP())", 6, params);
}
#endif
