#ifndef CROSSDO_WORKFLOW_FILES_H
#define CROSSDO_WORKFLOW_FILES_H

/* Included after common and before account/project handlers. Every binding is
 * committed with its parent submission. Bytes remain in the API's private store. */
static const char *wf_file_string(json_t *value, const char *key) {
    const char *text = json_string_value(json_object_get(value, key));
    return text ? text : "";
}

static bool wf_file_actor_active(json_t *actor) {
    json_t *status = json_object_get(actor, "user_status");
    return !strcmp(wf_file_string(actor, "account_state"), "active") &&
        json_is_integer(status) && json_integer_value(status) >= 0 && json_integer_value(status) < 8;
}

static bool wf_file_is_admin(json_t *actor) {
    return wf_admin_user(actor);
}

static const char *wf_file_id(WF_Context *ctx, json_t *body) {
    const char *id = wf_text(ctx, body, "id", 36);
    if (id && !wf_uuid(id)) { wf_fail(ctx, 400, "Invalid file ID"); return NULL; }
    return id;
}

static json_t *wf_file_row(WF_Context *ctx, const char *id, bool lock) {
    const char *params[] = {id};
    return wf_one(ctx, lock
        ? "SELECT id,owner_id,purpose,project_id,original_name,mime_type,byte_size,sha256,created_at FROM uploaded_file WHERE id=? FOR UPDATE"
        : "SELECT id,owner_id,purpose,project_id,original_name,mime_type,byte_size,sha256,created_at FROM uploaded_file WHERE id=?", 1, params);
}

static json_t *wf_file_metadata(json_t *row) {
    const char *id = wf_file_string(row, "id");
    char url[64]; snprintf(url, sizeof(url), "/api/files/%s", id);
    json_t *result = json_pack("{s:s,s:s,s:s,s:s}", "id", id,
        "name", wf_file_string(row, "original_name"), "mime_type", wf_file_string(row, "mime_type"), "url", url);
    if (result) json_object_set(result, "size", json_object_get(row, "byte_size"));
    return result;
}

static json_t *wf_file_register(WF_Context *ctx, json_t *body) {
    const char *id = wf_file_id(ctx, body), *actor_id = wf_actor_id(ctx, body);
    const char *purpose = wf_text(ctx, body, "purpose", 16);
    const char *name = wf_text(ctx, body, "name", 1020);
    const char *mime = wf_text(ctx, body, "mime_type", 40);
    const char *sha = wf_text(ctx, body, "sha256", 64);
    json_t *size_value = json_object_get(body, "size");
    if (!id || !actor_id || !purpose || !name || !mime || !sha) return NULL;
    bool application = !strcmp(purpose, "application"), project = !strcmp(purpose, "project"), report = !strcmp(purpose, "case");
    if (!application && !project && !report) return wf_fail(ctx, 400, "Unknown file purpose");
    if (strcmp(mime, "image/png") && strcmp(mime, "image/jpeg") && strcmp(mime, "application/pdf"))
        return wf_fail(ctx, 400, "Unsupported file type");
    size_t chars = 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        if (*p < 32 || *p == 127 || *p == '/' || *p == '\\') return wf_fail(ctx, 400, "Invalid file name");
        if ((*p & 0xc0) != 0x80) ++chars;
    }
    if (chars > 255 || strlen(sha) != 64) return wf_fail(ctx, 400, "Invalid file metadata");
    for (const char *p = sha; *p; ++p)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')))
            return wf_fail(ctx, 400, "Invalid file digest");
    if (!json_is_integer(size_value) || json_integer_value(size_value) < 1 ||
        json_integer_value(size_value) > 10485760) return wf_fail(ctx, 400, "File must be between 1 byte and 10 MiB");
    const char *project_id = project ? wf_text(ctx, body, "project_id", 36) : NULL;
    if (project && (!project_id || !wf_uuid(project_id))) return wf_fail(ctx, 400, "Invalid project ID");

    /* Serializing per owner also bounds total stored metadata-backed content. */
    json_t *actor = wf_user(ctx, actor_id, true);
    if (!actor) return NULL;
    if (!wf_file_actor_active(actor) || (!report && json_integer_value(json_object_get(actor, "user_permission")) != 3)) {
        json_decref(actor); return wf_fail(ctx, 403, "An active patient is required for application or project materials");
    }
    json_decref(actor);
    if (project) {
        const char *params[] = {project_id};
        json_t *owner = wf_one(ctx, "SELECT user_id FROM project WHERE project_id=?", 1, params);
        if (!owner) return NULL;
        bool owns = !strcmp(wf_file_string(owner, "user_id"), actor_id);
        json_decref(owner);
        if (!owns) return wf_fail(ctx, 403, "Only the project owner may upload its materials");
    }
    const char *quota_params[] = {actor_id};
    /* The router may have opened an older repeatable-read snapshot before the
     * owner lock. A locking aggregate must see uploads committed while waiting. */
    json_t *quota = wf_one(ctx, "SELECT CAST(COALESCE(SUM(byte_size),0) AS UNSIGNED) AS used FROM uploaded_file WHERE owner_id=? FOR UPDATE", 1, quota_params);
    if (!quota) return NULL;
    json_int_t used = json_integer_value(json_object_get(quota, "used"));
    json_decref(quota);
    if (used < 0 || used + json_integer_value(size_value) > 524288000)
        return wf_fail(ctx, 409, "The account upload quota has been reached");
    char size[24]; snprintf(size, sizeof(size), "%lld", (long long)json_integer_value(size_value));
    const char *params[] = {id, actor_id, purpose, project_id, id, name, mime, size, sha};
    if (!wf_exec(ctx, "INSERT INTO uploaded_file(id,owner_id,purpose,project_id,storage_key,original_name,mime_type,byte_size,sha256,created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,UTC_TIMESTAMP())", 9, params)) return NULL;
    json_t *row = wf_file_row(ctx, id, false);
    json_t *result = row ? wf_file_metadata(row) : NULL;
    json_decref(row); return result;
}

/* Returns true for legacy external references or for an exact managed file URL.
 * Callers may reuse this during shape validation, before knowing the parent ID. */
static bool wf_file_reference_valid(const char *reference) {
    if (!reference) return false;
    if (!strncmp(reference, "https://", 8) || !strncmp(reference, "http://", 7)) return true;
    return !strncmp(reference, "/api/files/", 11) && wf_uuid(reference + 11);
}

/* Validate owner-controlled references without publishing them. Case evidence
 * and pending payment evidence use this before any public usage grant exists. */
static bool wf_files_validate_refs(WF_Context *ctx, json_t *refs, const char *actor_id,
                                   const char *kind, const char *target) {
    if (!wf_uuid(actor_id) || !json_is_array(refs) || json_array_size(refs) > 100 ||
        (kind && strcmp(kind, "application") && strcmp(kind, "project")) ||
        (kind && (!wf_uuid(target) || (!strcmp(kind, "application") && strcmp(actor_id, target))))) {
        wf_fail(ctx, 400, "Invalid material references"); return false;
    }
    size_t index;
    json_t *value;
    json_array_foreach(refs, index, value) {
        const char *ref = json_string_value(value);
        if (!ref || strlen(ref) != json_string_length(value) || strlen(ref) > 2048 || !wf_file_reference_valid(ref)) {
            wf_fail(ctx, 400, "Invalid material reference"); return false;
        }
        if (*ref != '/') continue;
        json_t *file = wf_file_row(ctx, ref + 11, false);
        if (!file) return false;
        bool allowed = !strcmp(wf_file_string(file, "owner_id"), actor_id) &&
            (!kind || (!strcmp(wf_file_string(file, "purpose"), kind) &&
                (strcmp(kind, "project") || !strcmp(wf_file_string(file, "project_id"), target))));
        json_decref(file);
        if (!allowed) { wf_fail(ctx, 403, "The material does not belong to this applicant and purpose"); return false; }
    }
    return true;
}

static bool wf_files_owned_refs(WF_Context *ctx, json_t *refs, const char *actor_id) {
    return wf_files_validate_refs(ctx, refs, actor_id, NULL, NULL);
}

static bool wf_files_bind(WF_Context *ctx, json_t *refs, const char *actor_id,
                          const char *kind, const char *target, json_int_t version) {
    bool application = kind && !strcmp(kind, "application"), project = kind && !strcmp(kind, "project");
    if ((!application && !project) || !wf_uuid(actor_id) || !wf_uuid(target) ||
        !json_is_array(refs) || version < 0 || (uint64_t)version > UINT32_MAX ||
        (application && (strcmp(actor_id, target) || version != 0))) {
        wf_fail(ctx, 400, "Invalid file attachment target"); return false;
    }
    if (!wf_files_validate_refs(ctx, refs, actor_id, kind, target)) return false;
    size_t index;
    json_t *value;
    json_array_foreach(refs, index, value) {
        const char *ref = json_string_value(value);
        if (*ref != '/') continue;
        const char *id = ref + 11;
        char sequence[16]; snprintf(sequence, sizeof(sequence), "%lld", (long long)version);
        const char *params[] = {id, application ? target : NULL, project ? target : NULL, sequence};
        if (!wf_exec(ctx, "INSERT INTO file_usage(file_id,application_user_id,project_id,submission_no,created_at) "
            "VALUES(?,?,?,?,UTC_TIMESTAMP()) ON DUPLICATE KEY UPDATE file_id=VALUES(file_id)", 4, params)) return false;
    }
    return true;
}

static json_t *wf_file_access(WF_Context *ctx, json_t *body) {
    const char *id = wf_file_id(ctx, body);
    if (!id) return NULL;
    json_t *actor_value = json_object_get(body, "actor_id");
    const char *actor_id = json_string_value(actor_value);
    if (actor_value && (!actor_id || !wf_uuid(actor_id))) return wf_fail(ctx, 400, "Invalid actor ID");
    json_t *actor = actor_id ? wf_user(ctx, actor_id, false) : NULL;
    if (actor_id && !actor) return NULL;
    if (actor && !wf_file_actor_active(actor)) { json_decref(actor); return wf_fail(ctx, 403, "Account is not active"); }
    json_t *file = wf_file_row(ctx, id, false);
    if (!file) { json_decref(actor); return NULL; }
    bool allowed = actor && (!strcmp(actor_id, wf_file_string(file, "owner_id")) || wf_file_is_admin(actor));
    if (!allowed && actor && wf_is_volunteer(actor)) {
        const char *params[] = {id, actor_id, id, actor_id};
        json_t *access = wf_one(ctx,
            "SELECT (EXISTS(SELECT 1 FROM file_usage f JOIN patient_application a ON a.user_id=f.application_user_id "
            "WHERE f.file_id=? AND a.current_volunteer=?) OR "
            "EXISTS(SELECT 1 FROM file_usage f JOIN project p ON p.project_id=f.project_id "
            "WHERE f.file_id=? AND p.current_volunteer=?)) AS allowed", 4, params);
        if (!access) { json_decref(actor); json_decref(file); return NULL; }
        allowed = json_integer_value(json_object_get(access, "allowed")) == 1;
        json_decref(access);
    }
    if (!allowed && !strcmp(wf_file_string(file, "purpose"), "project")) {
        const char *params[] = {id};
        json_t *access = wf_one(ctx,
            "SELECT EXISTS(SELECT 1 FROM file_usage f JOIN project p ON p.project_id=f.project_id "
            "WHERE f.file_id=? AND f.submission_no=p.submission_no AND p.project_status IN (0,13)) AS allowed", 1, params);
        if (!access) { json_decref(actor); json_decref(file); return NULL; }
        allowed = json_integer_value(json_object_get(access, "allowed")) == 1;
        json_decref(access);
    }
    json_decref(actor);
    if (!allowed) { json_decref(file); return wf_fail(ctx, 403, "This material is private"); }
    json_t *result = wf_file_metadata(file);
    json_decref(file); return result;
}

#endif
