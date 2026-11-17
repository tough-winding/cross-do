#ifndef CROSSDO_WORKFLOW_ACCOUNTS_H
#define CROSSDO_WORKFLOW_ACCOUNTS_H

/* These operations run inside the transaction owned by the workflow router. */

static const char *wa_string(json_t *value, const char *key) {
    const char *text = json_string_value(json_object_get(value, key));
    return text ? text : "";
}

static int wa_integer(json_t *value, const char *key) {
    return (int)json_integer_value(json_object_get(value, key));
}

static bool wa_phone(const char *phone) {
    if (!phone || strlen(phone) != 11 || phone[0] != '1' || phone[1] < '3' || phone[1] > '9') return false;
    for (int i = 2; i < 11; i++) if (phone[i] < '0' || phone[i] > '9') return false;
    return true;
}

static bool wa_id_card(const char *card) {
    if (!card || strlen(card) != 18) return false;
    for (int i = 0; i < 17; i++) if (card[i] < '0' || card[i] > '9') return false;
    return (card[17] >= '0' && card[17] <= '9') || card[17] == 'X' || card[17] == 'x';
}

static const char *wa_text(WF_Context *ctx, json_t *body, const char *key, size_t max_chars) {
    const char *value = wf_text(ctx, body, key, max_chars * 4);
    if (!value) return NULL;
    size_t chars = 0;
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) if ((*p & 0xc0) != 0x80) chars++;
    if (chars > max_chars) {
        wf_fail(ctx, 400, "Text exceeds the field length");
        return NULL;
    }
    return value;
}

static json_t *wa_public_account(json_t *user) {
    static const char *keys[] = {
        "user_id", "user_permission", "user_name", "avatar", "real_name", "personal_note",
        "real_name_authentication", "user_status", "existing_projects", "project_id", "phone_number",
        "user_age", "registration_date", "login_time", "total_donation_amount", "account_state", "phone_verified"
    };
    json_t *result = json_object();
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        json_t *value = json_object_get(user, keys[i]);
        if (value) json_object_set(result, keys[i], value);
    }
    return result;
}

static json_t *wa_actor_checked(WF_Context *ctx, json_t *body, bool volunteer, bool lock) {
    const char *actor_id = wf_actor_id(ctx, body);
    if (!actor_id) return NULL;
    json_t *actor = wf_user(ctx, actor_id, lock);
    if (!actor) return NULL;
    if (!wf_active_account(actor)) {
        json_decref(actor); return wf_fail(ctx, 403, "Account is frozen");
    }
    if (volunteer && !wf_is_volunteer(actor)) {
        json_decref(actor);
        return wf_fail(ctx, 403, "An active verified volunteer account is required");
    }
    return actor;
}

static json_t *wa_actor(WF_Context *ctx, json_t *body, bool volunteer) {
    return wa_actor_checked(ctx, body, volunteer, true);
}

static const char *wa_target_id(WF_Context *ctx, json_t *body) {
    const char *id = wf_text(ctx, body, "user_id", 36);
    if (!id) return NULL;
    if (!wf_uuid(id)) {
        wf_fail(ctx, 400, "user_id must be a UUID");
        return NULL;
    }
    return id;
}

static json_t *wa_application(WF_Context *ctx, const char *user_id, bool lock) {
    const char *params[] = {user_id};
    return wf_one(ctx, lock
        ? "SELECT user_id,materials,current_volunteer,previous_volunteers,submitted_at,last_change_time "
          "FROM patient_application WHERE user_id = ? FOR UPDATE"
        : "SELECT user_id,materials,current_volunteer,previous_volunteers,submitted_at,last_change_time "
          "FROM patient_application WHERE user_id = ?", 1, params);
}

static json_t *wa_application_result(WF_Context *ctx, json_t *user, json_t *application, bool details) {
    json_t *result = json_object();
    json_object_set_new(result, "account", wa_public_account(user));
    json_object_set(result, "user_id", json_object_get(user, "user_id"));
    json_object_set(result, "user_status", json_object_get(user, "user_status"));
    if (application) {
        static const char *keys[] = {"current_volunteer", "submitted_at", "last_change_time"};
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            json_t *value = json_object_get(application, keys[i]);
            if (value) json_object_set(result, keys[i], value);
        }
        if (details) {
            const char *id = wa_string(user, "user_id");
            json_t *materials = wf_json_array(ctx, wa_string(application, "materials"));
            json_t *previous = materials ? wf_history_volunteers(ctx, false, id,
                wa_string(application, "previous_volunteers")) : NULL;
            json_t *history = previous ? wf_history_read(ctx, false, id) : NULL;
            if (!materials || !previous || !history) {
                json_decref(materials); json_decref(previous); json_decref(history);
                json_decref(result); return NULL;
            }
            json_object_set_new(result, "materials", materials);
            json_object_set_new(result, "previous_volunteers", previous);
            json_object_set_new(result, "verification_record", history);
        }
    }
    return result;
}

static json_t *wf_register(WF_Context *ctx, json_t *body) {
    if (!json_is_true(json_object_get(body, "phone_verified")))
        return wf_fail(ctx, 400, "Verified phone is required");
    const char *phone = wf_text(ctx, body, "phone_number", 11);
    const char *name = wa_text(ctx, body, "user_name", 24);
    const char *hash = wf_text(ctx, body, "password_hash", 64);
    json_t *permission_value = json_object_get(body, "user_permission");
    if (!phone || !name || !hash) return NULL;
    if (!wa_phone(phone) || !json_is_integer(permission_value)) return wf_fail(ctx, 400, "Invalid registration fields");
    json_int_t permission_number = json_integer_value(permission_value);
    if (permission_number != 2 && permission_number != 3 && permission_number != 5)
        return wf_fail(ctx, 400, "This role cannot be self-registered");
    int permission = (int)permission_number;
    /* Java supplies BCrypt; reject plaintext and malformed hashes at this boundary too. */
    if (strlen(hash) != 60 || (strncmp(hash, "$2a$", 4) && strncmp(hash, "$2b$", 4) && strncmp(hash, "$2y$", 4)))
        return wf_fail(ctx, 400, "A BCrypt password hash is required");
    char id[37], role[4], status[4];
    wf_new_id(id);
    snprintf(role, sizeof(role), "%d", permission);
    snprintf(status, sizeof(status), "%d", permission == 2 ? 0 : 1);
    const char *params[] = {id, role, name, hash, phone, status};
    if (!wf_exec(ctx, "INSERT INTO user (user_id,user_permission,user_name,user_password,phone_number,user_status,"
        "real_name_authentication,existing_projects,registration_date,total_donation_amount,phone_verified) "
        "VALUES (?,?,?,?,?,?,0,0,UTC_TIMESTAMP(),0.00,1)", 6, params)) return NULL;
    json_t *user = wf_user(ctx, id, false);
    if (!user) return NULL;
    json_t *result = wa_public_account(user);
    json_decref(user);
    return result;
}

static json_t *wf_credentials(WF_Context *ctx, json_t *body) {
    const char *phone = wf_text(ctx, body, "phone_number", 11);
    if (!phone) return NULL;
    if (!wa_phone(phone)) return wf_fail(ctx, 400, "Invalid phone_number");
    const char *params[] = {phone};
    /* Internal service endpoint only; the Java login handler never serializes this result. */
    return wf_one(ctx, "SELECT user_id,user_password,user_status,user_permission,real_name_authentication,account_state,session_version "
        "FROM user WHERE phone_number = ?", 1, params);
}

static json_t *wf_account_get(WF_Context *ctx, json_t *body) {
    json_t *user = wa_actor_checked(ctx, body, false, false);
    if (!user) return NULL;
    json_t *result = wa_public_account(user);
    json_decref(user);
    return result;
}

static json_t *wf_application_submit(WF_Context *ctx, json_t *body) {
    const char *real_name = wa_text(ctx, body, "real_name", 20);
    const char *id_card = wf_text(ctx, body, "id_card", 18);
    json_t *age_value = json_object_get(body, "user_age");
    json_t *materials = json_object_get(body, "materials");
    if (!real_name || !id_card) return NULL;
    if (!wa_id_card(id_card) || !json_is_integer(age_value) || json_integer_value(age_value) < 0 ||
        json_integer_value(age_value) > 127 || !json_is_array(materials) || json_array_size(materials) == 0 ||
        json_array_size(materials) > 30) return wf_fail(ctx, 400, "Identity, age, and material references are required");
    size_t index;
    json_t *item;
    json_array_foreach(materials, index, item) {
        const char *ref = json_string_value(item);
        if (!ref || json_string_length(item) != strlen(ref) || strlen(ref) > 2048 ||
            !wf_reference(ref))
            return wf_fail(ctx, 400, "Materials must contain uploaded files or HTTP(S) references");
    }
    json_t *user = wa_actor(ctx, body, false);
    if (!user) return NULL;
    int status = wa_integer(user, "user_status");
    if (wa_integer(user, "user_permission") != 3) {
        json_decref(user);
        return wf_fail(ctx, 403, "Only patient accounts can submit an application");
    }
    if (status != 1 && status != 4 && status != 5) {
        json_decref(user);
        return wf_fail(ctx, 409, "This account cannot submit a patient application in its current state");
    }
    const char *user_id = wa_string(user, "user_id");
    /* The patient row is the lock shared by every application transition. The
     * upsert below acquires its application row before the event is appended. */
    char *materials_text = wf_dump(ctx, materials);
    if (!materials_text) { json_decref(user); return NULL; }
    char age[8], new_status[4];
    snprintf(age, sizeof(age), "%lld", (long long)json_integer_value(age_value));
    snprintf(new_status, sizeof(new_status), "%d", status == 1 ? 2 : 6);
    const char *update_user[] = {real_name, id_card, age, new_status, user_id};
    bool ok = wf_exec(ctx, "UPDATE user SET real_name=?,id_card=?,user_age=?,user_status=?,"
        "real_name_authentication=0 WHERE user_id=?", 5, update_user);
    if (ok) {
        const char *update_app[] = {user_id, materials_text};
        ok = wf_exec(ctx, "INSERT INTO patient_application (user_id,materials,current_volunteer,previous_volunteers,"
            "verification_record,submitted_at,last_change_time) VALUES (?,?,NULL,'[]','[]',UTC_TIMESTAMP(),UTC_TIMESTAMP()) "
            "ON DUPLICATE KEY UPDATE materials=VALUES(materials),"
            "current_volunteer=NULL,submitted_at=UTC_TIMESTAMP(),last_change_time=UTC_TIMESTAMP()", 2, update_app);
    }
    if (ok) {
        ok = wf_files_bind(ctx, materials, user_id, "application", user_id, 0);
    }
    if (ok) {
        json_t *extra = json_pack("{s:O}", "materials", materials);
        if (!extra) { wf_fail(ctx, 500, "Unable to serialize review event"); ok = false; }
        else ok = wf_history_append(ctx, false, user_id, user_id, "submit", "Patient materials submitted", extra);
        json_decref(extra);
    }
    free(materials_text);
    if (!ok) { json_decref(user); return NULL; }
    json_object_set_new(user, "user_status", json_integer(status == 1 ? 2 : 6));
    json_object_set_new(user, "real_name", json_string(real_name));
    json_object_set_new(user, "user_age", json_integer(json_integer_value(age_value)));
    json_object_set_new(user, "real_name_authentication", json_integer(0));
    json_t *app = wa_application(ctx, user_id, false);
    json_t *result = app ? wa_application_result(ctx, user, app, true) : NULL;
    json_decref(app); json_decref(user);
    return result;
}

static bool wa_page(WF_Context *ctx, json_t *body, char limit[8], char offset[16]) {
    json_t *limit_value = json_object_get(body, "limit"), *offset_value = json_object_get(body, "offset");
    json_int_t l = limit_value ? json_integer_value(limit_value) : 20;
    json_int_t o = offset_value ? json_integer_value(offset_value) : 0;
    if ((limit_value && !json_is_integer(limit_value)) || (offset_value && !json_is_integer(offset_value)) ||
        l < 1 || l > 100 || o < 0 || o > 1000000) {
        wf_fail(ctx, 400, "Pagination requires limit 1..100 and offset 0..1000000");
        return false;
    }
    snprintf(limit, 8, "%lld", (long long)l);
    snprintf(offset, 16, "%lld", (long long)o);
    return true;
}

static json_t *wf_application_list(WF_Context *ctx, json_t *body) {
    json_t *actor = wa_actor_checked(ctx, body, true, false);
    if (!actor) return NULL;
    char limit[8], offset[16];
    if (!wa_page(ctx, body, limit, offset)) { json_decref(actor); return NULL; }
    /* The pool contains only summaries. Sensitive materials require claiming first. */
    const char *actor_id = wa_string(actor, "user_id");
    char window[16];
    snprintf(window, sizeof(window), "%ld", strtol(limit, NULL, 10) + strtol(offset, NULL, 10));
    const char *params[] = {actor_id, window, actor_id, actor_id, window, limit, offset};
    /* Pending applications and this reviewer's claimed work are disjoint. Page
     * their narrow keys first; only the final page needs full display profiles.
     * Each branch keeps offset+limit rows so global pagination stays identical. */
    json_t *result = wf_query(ctx, "SELECT a.user_id,u.user_name,u.user_age,u.user_status,a.current_volunteer,"
        "a.submitted_at,a.last_change_time FROM (SELECT user_id,submitted_at FROM ("
        "(SELECT a.user_id,a.submitted_at FROM patient_application a JOIN user u ON a.user_id=u.user_id "
        "WHERE u.user_permission=3 AND u.user_id<>? AND u.user_status IN (2,6) "
        "ORDER BY a.submitted_at,a.user_id LIMIT ?) UNION ALL "
        "(SELECT a.user_id,a.submitted_at FROM patient_application a JOIN user u ON a.user_id=u.user_id "
        "WHERE u.user_permission=3 AND u.user_id<>? AND u.user_status IN (3,7) AND a.current_volunteer=? "
        "ORDER BY a.submitted_at,a.user_id LIMIT ?)) candidates "
        "ORDER BY submitted_at,user_id LIMIT ? OFFSET ?) page "
        "JOIN user u ON u.user_id=page.user_id JOIN patient_application a ON a.user_id=page.user_id "
        "ORDER BY page.submitted_at,page.user_id", 7, params);
    json_decref(actor);
    return result;
}

static json_t *wf_application_get(WF_Context *ctx, json_t *body) {
    const char *target_id = wa_target_id(ctx, body);
    if (!target_id) return NULL;
    json_t *actor = wa_actor_checked(ctx, body, false, false);
    if (!actor) return NULL;
    json_t *user = wf_user(ctx, target_id, false);
    json_t *app = user ? wa_application(ctx, target_id, false) : NULL;
    if (!user || !app) { json_decref(actor); json_decref(user); json_decref(app); return NULL; }
    const char *actor_id = wa_string(actor, "user_id");
    if (strcmp(actor_id, target_id) && (!wf_is_volunteer(actor) || strcmp(actor_id, wa_string(app, "current_volunteer")))) {
        json_decref(actor); json_decref(user); json_decref(app);
        return wf_fail(ctx, 403, "Only the patient and assigned reviewer can read this application");
    }
    json_t *result = wa_application_result(ctx, user, app, true);
    json_decref(actor); json_decref(user); json_decref(app);
    return result;
}

static json_t *wf_application_claim(WF_Context *ctx, json_t *body) {
    const char *target_id = wa_target_id(ctx, body);
    if (!target_id) return NULL;
    json_t *actor = wa_actor(ctx, body, true);
    if (!actor) return NULL;
    const char *actor_id = wa_string(actor, "user_id");
    if (!strcmp(actor_id, target_id)) { json_decref(actor); return wf_fail(ctx, 403, "Applicants cannot review themselves"); }
    json_t *user = wf_user(ctx, target_id, true);
    json_t *app = user ? wa_application(ctx, target_id, true) : NULL;
    if (!user || !app) { json_decref(actor); json_decref(user); json_decref(app); return NULL; }
    int status = wa_integer(user, "user_status");
    if (wa_integer(user, "user_permission") != 3 || (status != 2 && status != 6) || *wa_string(app, "current_volunteer")) {
        json_decref(actor); json_decref(user); json_decref(app);
        return wf_fail(ctx, 409, "This application is not available to claim");
    }
    const char *assignment[] = {actor_id, target_id};
    bool ok = wf_exec(ctx, "UPDATE patient_application SET current_volunteer=?,"
        "last_change_time=UTC_TIMESTAMP() WHERE user_id=?", 2, assignment);
    if (ok) {
        const char *params[] = {status == 2 ? "3" : "7", target_id};
        ok = wf_exec(ctx, "UPDATE user SET user_status=? WHERE user_id=?", 2, params);
    }
    if (ok) ok = wf_history_append(ctx, false, target_id, actor_id, "claim", "Volunteer claimed review", NULL);
    json_decref(actor); json_decref(app);
    if (!ok) { json_decref(user); return NULL; }
    json_object_set_new(user, "user_status", json_integer(status == 2 ? 3 : 7));
    app = wa_application(ctx, target_id, false);
    json_t *result = app ? wa_application_result(ctx, user, app, true) : NULL;
    json_decref(user); json_decref(app);
    return result;
}

static json_t *wf_application_review(WF_Context *ctx, json_t *body) {
    const char *target_id = wa_target_id(ctx, body);
    const char *decision = wf_text(ctx, body, "decision", 20);
    const char *note = wf_text(ctx, body, "note", 4000);
    if (!target_id || !decision || !note) return NULL;
    int new_status;
    if (!strcmp(decision, "approve")) new_status = 0;
    else if (!strcmp(decision, "correction")) new_status = 5;
    else if (!strcmp(decision, "unreachable")) new_status = 4;
    else return wf_fail(ctx, 400, "Unknown review decision");
    json_t *actor = wa_actor(ctx, body, true);
    if (!actor) return NULL;
    const char *actor_id = wa_string(actor, "user_id");
    if (!strcmp(actor_id, target_id)) { json_decref(actor); return wf_fail(ctx, 403, "Applicants cannot review themselves"); }
    json_t *user = wf_user(ctx, target_id, true);
    json_t *app = user ? wa_application(ctx, target_id, true) : NULL;
    if (!user || !app) { json_decref(actor); json_decref(user); json_decref(app); return NULL; }
    int status = wa_integer(user, "user_status");
    if (strcmp(actor_id, wa_string(app, "current_volunteer"))) {
        json_decref(actor); json_decref(user); json_decref(app);
        return wf_fail(ctx, 403, "Only the assigned volunteer may review this application");
    }
    if (status != 3 && status != 7) {
        json_decref(actor); json_decref(user); json_decref(app);
        return wf_fail(ctx, 409, "This application is not under review");
    }
    const char *release[] = {target_id};
    bool ok = wf_exec(ctx, "UPDATE patient_application SET current_volunteer=NULL,"
        "last_change_time=UTC_TIMESTAMP() WHERE user_id=?", 1, release);
    if (ok) {
        char next[4]; snprintf(next, sizeof(next), "%d", new_status);
        const char *params[] = {next, new_status == 0 ? "1" : "0", target_id};
        ok = wf_exec(ctx, "UPDATE user SET user_status=?,real_name_authentication=? WHERE user_id=?", 3, params);
    }
    if (ok) ok = wf_history_append(ctx, false, target_id, actor_id, decision, note, NULL);
    json_decref(actor); json_decref(app);
    if (!ok) { json_decref(user); return NULL; }
    json_object_set_new(user, "user_status", json_integer(new_status));
    json_object_set_new(user, "real_name_authentication", json_integer(new_status == 0 ? 1 : 0));
    app = wa_application(ctx, target_id, false);
    json_t *result = app ? wa_application_result(ctx, user, app, true) : NULL;
    json_decref(user); json_decref(app);
    return result;
}

#endif
