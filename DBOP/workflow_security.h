#ifndef CROSSDO_WORKFLOW_SECURITY_H
#define CROSSDO_WORKFLOW_SECURITY_H

/* All actions are internal service-only. OTP validation and BCrypt verification
 * happen in Java; these transaction handlers enforce identity/version races. */
static bool ws_active(json_t *user) {
    json_t *status = json_object_get(user, "user_status");
    return !strcmp(wa_string(user, "account_state"), "active") &&
        json_is_integer(status) && json_integer_value(status) >= 0 && json_integer_value(status) < 8;
}

static const char *ws_version(WF_Context *ctx, json_t *body, const char *field) {
    const char *version = wf_text(ctx, body, field, 20);
    if (!version) return NULL;
    for (const char *p = version; *p; ++p) {
        if (*p < '0' || *p > '9') { wf_fail(ctx, 400, "Invalid session version"); return NULL; }
    }
    if (strlen(version) > 1 && *version == '0') { wf_fail(ctx, 400, "Invalid session version"); return NULL; }
    return version;
}

static bool ws_hash(const char *hash) {
    return hash && strlen(hash) == 60 &&
        (!strncmp(hash,"$2a$",4) || !strncmp(hash,"$2b$",4) || !strncmp(hash,"$2y$",4));
}

static json_t *wf_security_version(WF_Context *ctx, json_t *body) {
    const char *id = wf_actor_id(ctx, body);
    if (!id) return NULL;
    json_t *user = wf_user(ctx, id, false);
    if (!user) return NULL;
    if (!ws_active(user)) { json_decref(user); return wf_fail(ctx,403,"Account is not active"); }
    json_t *result = json_object();
    json_object_set(result,"user_id",json_object_get(user,"user_id"));
    json_object_set(result,"session_version",json_object_get(user,"session_version"));
    json_decref(user);
    return result;
}

static json_t *wf_security_session(WF_Context *ctx, json_t *body) {
    const char *id = wf_actor_id(ctx,body);
    const char *version = ws_version(ctx,body,"session_version");
    if (!id || !version) return NULL;
    const char *params[] = {id,version};
    json_t *rows = wf_query(ctx,"SELECT user_id,user_status,account_state FROM user WHERE user_id=? AND session_version=?",2,params);
    if (!rows) return NULL;
    json_t *user = json_array_get(rows,0);
    /* DBOP reserves 401 for service authentication (the client may replay it).
     * Java maps this authorization failure to an end-user 401 without retry. */
    if (!user) { json_decref(rows); return wf_fail(ctx,403,"Session was revoked; sign in again"); }
    if (!ws_active(user)) { json_decref(rows); return wf_fail(ctx,403,"Account is not active"); }
    json_t *result = json_pack("{s:s}","user_id",id);
    json_decref(rows);
    return result;
}

static json_t *wf_security_credentials(WF_Context *ctx,json_t *body) {
    const char *id = wf_actor_id(ctx,body);
    if (!id) return NULL;
    const char *params[] = {id};
    return wf_one(ctx,"SELECT user_id,user_password,session_version FROM user WHERE user_id=?",1,params);
}

static json_t *wf_security_reset_password(WF_Context *ctx,json_t *body) {
    const char *phone = wf_text(ctx,body,"phone_number",11);
    const char *hash = wf_text(ctx,body,"password_hash",64);
    if (!phone || !hash) return NULL;
    if (!wa_phone(phone) || !ws_hash(hash)) return wf_fail(ctx,400,"Invalid password reset fields");
    const char *find[] = {phone};
    json_t *rows = wf_query(ctx,"SELECT user_id FROM user WHERE phone_number=? FOR UPDATE",1,find);
    if (!rows) return NULL;
    json_t *user = json_array_get(rows,0);
    bool ok = true;
    if (user) {
        const char *id = wa_string(user,"user_id");
        const char *params[] = {hash,id};
        ok = wf_exec(ctx,"UPDATE user SET user_password=?,phone_verified=1,session_version=session_version+1 WHERE user_id=?",2,params);
        if (ok) ok = wf_notify(ctx,id,"security","密码已重置","您的账户密码已重置，所有旧登录已失效。","/account");
    }
    json_decref(rows);
    return ok ? json_pack("{s:b}","updated",1) : NULL;
}

static json_t *wf_security_change_phone(WF_Context *ctx,json_t *body) {
    const char *id = wf_actor_id(ctx,body);
    const char *phone = wf_text(ctx,body,"phone_number",11);
    const char *hash = wf_text(ctx,body,"expected_password_hash",64);
    const char *version = ws_version(ctx,body,"expected_session_version");
    if (!id || !phone || !hash || !version) return NULL;
    if (!wa_phone(phone) || !ws_hash(hash)) return wf_fail(ctx,400,"Invalid phone change fields");
    const char *find[] = {id};
    json_t *user = wf_one(ctx,"SELECT user_id,user_password,session_version,account_state,user_status FROM user WHERE user_id=? FOR UPDATE",1,find);
    if (!user) return NULL;
    if (!ws_active(user)) { json_decref(user); return wf_fail(ctx,403,"Account is not active"); }
    char stored_version[32];
    snprintf(stored_version,sizeof(stored_version),"%lld",(long long)json_integer_value(json_object_get(user,"session_version")));
    if (strcmp(hash,wa_string(user,"user_password")) || strcmp(version,stored_version)) {
        json_decref(user); return wf_fail(ctx,409,"Credentials changed; sign in again before changing phone");
    }
    const char *params[] = {phone,id};
    bool ok = wf_exec(ctx,"UPDATE user SET phone_number=?,phone_verified=1,session_version=session_version+1 WHERE user_id=?",2,params);
    if (ok) ok = wf_notify(ctx,id,"security","手机号已更换","账户手机号已更换，所有旧登录已失效，请使用新手机号登录。","/account");
    json_decref(user);
    return ok ? json_pack("{s:b}","updated",1) : NULL;
}
#endif
