#ifndef CROSSDO_WORKFLOW_PROJECTS_H
#define CROSSDO_WORKFLOW_PROJECTS_H

/* Business transitions run in the transaction opened by workflow_router.h. */
static const char *wf_project_id(WF_Context *ctx, json_t *body) {
    const char *id = wf_text(ctx, body, "project_id", 36);
    if (id && !wf_uuid(id)) {
        wf_fail(ctx, 400, "project_id must be a UUID");
        return NULL;
    }
    return id;
}

static json_t *wf_project_actor(WF_Context *ctx, json_t *body, bool lock) {
    const char *id = wf_actor_id(ctx, body);
    if (!id) return NULL;
    json_t *user = wf_user(ctx, id, lock);
    if (!user) return NULL;
    json_t *status = json_object_get(user, "user_status");
    if (!wf_active_account(user) || !json_is_integer(status) || json_integer_value(status) >= 8 || json_integer_value(status) < 0) {
        json_decref(user);
        return wf_fail(ctx, 403, "account is not active");
    }
    return user;
}

static bool wf_project_paging(WF_Context *ctx, json_t *body, char limit[16], char offset[16]) {
    json_t *l = json_object_get(body, "limit"), *o = json_object_get(body, "offset");
    if ((l && (!json_is_integer(l) || json_integer_value(l) < 1 || json_integer_value(l) > 100)) ||
        (o && (!json_is_integer(o) || json_integer_value(o) < 0 || json_integer_value(o) > 1000000))) {
        wf_fail(ctx, 400, "limit must be 1..100 and offset must be 0..1000000");
        return false;
    }
    snprintf(limit, 16, "%lld", (long long)(l ? json_integer_value(l) : 20));
    snprintf(offset, 16, "%lld", (long long)(o ? json_integer_value(o) : 0));
    return true;
}

/* Only opaque HTTP(S) object references are accepted, never server file paths. */
static bool wf_project_entry_valid(WF_Context *ctx, json_t *entry) {
    if (!json_is_object(entry)) {
        wf_fail(ctx, 400, "pathography entry must be an object");
        return false;
    }
    const char *content = wf_text(ctx, entry, "content", 12000);
    if (!content) return false;
    bool nonspace = false;
    for (const unsigned char *p = (const unsigned char *)content; *p; ++p)
        if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') nonspace = true;
    if (!nonspace) {
        wf_fail(ctx, 400, "pathography content cannot be blank");
        return false;
    }
    json_t *refs = json_object_get(entry, "attachments");
    if (!json_is_array(refs) || json_array_size(refs) > 20) {
        wf_fail(ctx, 400, "attachments must be an array of at most 20 object references");
        return false;
    }
    size_t index;
    json_t *ref;
    json_array_foreach(refs, index, ref) {
        const char *s = json_string_value(ref);
        if (!s || json_string_length(ref) != strlen(s) || strlen(s) > 2048 ||
            !wf_reference(s)) {
            wf_fail(ctx, 400, "attachments must contain uploaded files or HTTP(S) references");
            return false;
        }
    }
    return true;
}

static json_t *wf_project_clean_entry(WF_Context *ctx, json_t *entry) {
    if (!wf_project_entry_valid(ctx, entry)) return NULL;
    char now[20];
    wf_now(now);
    return json_pack("{s:s,s:O,s:s}", "content", json_string_value(json_object_get(entry, "content")),
                     "attachments", json_object_get(entry, "attachments"), "time", now);
}

static json_t *wf_project_clean_pathography(WF_Context *ctx, json_t *value) {
    if (!json_is_array(value) || json_array_size(value) < 1 || json_array_size(value) > 100) {
        return wf_fail(ctx, 400, "pathography must contain 1..100 entries");
    }
    json_t *clean = json_array();
    size_t i;
    json_t *entry;
    json_array_foreach(value, i, entry) {
        json_t *item = wf_project_clean_entry(ctx, entry);
        if (!item) {
            json_decref(clean);
            return NULL;
        }
        json_array_append_new(clean, item);
    }
    return clean;
}

static json_t *wf_project_row(WF_Context *ctx, const char *id, bool lock) {
    const char *params[] = {id};
    /* Legacy text snapshots and generated relation keys are not runtime data. */
    return wf_one(ctx, lock
        ? "SELECT project_id,user_id,user_name,user_age,real_name,project_status,current_volunteer,"
          "previous_volunteers,question_is_volunteer_assigned,current_dispute_id,project_create_time,"
          "donation_pool_limit,current_donations_received,current_used_donations,last_update_time,submission_no "
          "FROM project WHERE project_id=? FOR UPDATE"
        : "SELECT project_id,user_id,user_name,user_age,real_name,project_status,current_volunteer,"
          "previous_volunteers,question_is_volunteer_assigned,current_dispute_id,project_create_time,"
          "donation_pool_limit,current_donations_received,current_used_donations,last_update_time,submission_no "
          "FROM project WHERE project_id=?", 1, params);
}

/* The project row is the serialization point for version changes and entry appends.
 * Individual entries avoid rewriting the entire accumulated medical history. */
static bool wf_project_append_entry(WF_Context *ctx, const char *id, const char *version, json_t *entry) {
    char *encoded = json_dumps(entry, JSON_COMPACT);
    if (!encoded) { wf_fail(ctx, 500, "Unable to serialize project entry"); return false; }
    if (strlen(encoded) > 1024 * 1024) {
        free(encoded); wf_fail(ctx, 400, "Project entry exceeds the storage limit"); return false;
    }
    /* The parent lock already serializes this version. A normal consistent read
     * avoids INSERT ... SELECT's source gap locks on unrelated empty histories. */
    const char *lookup[] = {id, version};
    json_t *last = wf_one(ctx,
        "SELECT CAST(COALESCE(MAX(sequence_no),0) AS UNSIGNED) AS sequence_no "
        "FROM project_pathography_entry WHERE project_id=? AND submission_no=?", 2, lookup);
    if (!last) { free(encoded); return false; }
    json_t *value = json_object_get(last, "sequence_no");
    json_int_t previous = json_integer_value(value);
    if (!json_is_integer(value) || previous < 0 || previous == LLONG_MAX) {
        json_decref(last); free(encoded);
        wf_fail(ctx, 500, "Invalid project entry sequence"); return false;
    }
    char sequence[32];
    snprintf(sequence, sizeof(sequence), "%lld", (long long)previous + 1);
    json_decref(last);
    const char *params[] = {id, version, sequence, encoded};
    bool ok = wf_exec(ctx,
        "INSERT INTO project_pathography_entry(project_id,submission_no,sequence_no,record) "
        "VALUES(?,?,?,?)", 4, params);
    free(encoded);
    return ok;
}

static json_t *wf_project_entry_records(WF_Context *ctx, json_t *rows) {
    json_t *result = json_array();
    if (!result) return wf_fail(ctx, 500, "Unable to allocate project entries");
    size_t i;
    json_t *row;
    json_array_foreach(rows, i, row) {
        const char *encoded = json_string_value(json_object_get(row, "record"));
        json_t *entry = encoded ? json_loads(encoded, JSON_DECODE_ANY, NULL) : NULL;
        if (!entry || json_array_append_new(result, entry)) {
            json_decref(result);
            return wf_fail(ctx, 500, "Invalid stored project entry");
        }
    }
    return result;
}

static json_t *wf_project_entries(WF_Context *ctx, const char *id, json_t *version) {
    if (!json_is_integer(version) || json_integer_value(version) < 0 ||
        (uint64_t)json_integer_value(version) > UINT32_MAX)
        return wf_fail(ctx, 500, "Invalid stored project version");
    char value[16];
    snprintf(value, sizeof(value), "%lld", (long long)json_integer_value(version));
    const char *params[] = {id, value};
    json_t *rows = wf_query(ctx,
        "SELECT record FROM project_pathography_entry WHERE project_id=? AND submission_no=? "
        "ORDER BY sequence_no", 2, params);
    if (!rows) return NULL;
    json_t *entries = wf_project_entry_records(ctx, rows);
    json_decref(rows);
    return entries;
}

static bool wf_project_status_is(json_t *row, int status) {
    json_t *value = json_object_get(row, "project_status");
    return json_is_integer(value) && json_integer_value(value) == status;
}

/* Whitelist both views so generated database keys never become API fields. */
static json_t *wf_project_view(WF_Context *ctx, json_t *row, bool private_view) {
    static const char *public_keys[] = {
        "project_id", "user_name", "user_age", "project_status", "project_create_time",
        "donation_pool_limit", "current_donations_received", "current_used_donations", "last_update_time"
    };
    static const char *private_keys[] = {
        "user_id", "real_name", "current_volunteer", "question_is_volunteer_assigned",
        "current_dispute_id", "submission_no"
    };
    json_t *view = json_object();
    for (size_t i = 0; i < sizeof(public_keys) / sizeof(public_keys[0]); ++i) {
        json_t *value = json_object_get(row, public_keys[i]);
        if (value) json_object_set(view, public_keys[i], value);
    }
    const char *id = json_string_value(json_object_get(row, "project_id"));
    json_t *entries = wf_project_entries(ctx, id, json_object_get(row, "submission_no"));
    bool ok = entries != NULL;
    if (ok) json_object_set_new(view, "pathography", entries);
    if (ok && private_view) {
        for (size_t i = 0; i < sizeof(private_keys) / sizeof(private_keys[0]); ++i) {
            json_t *value = json_object_get(row, private_keys[i]);
            if (value) json_object_set(view, private_keys[i], value);
        }
        json_t *history = wf_history_read(ctx, true, id);
        json_t *volunteers = history ? wf_history_volunteers(ctx, true, id,
            json_string_value(json_object_get(row, "previous_volunteers"))) : NULL;
        ok = history && volunteers;
        if (ok) {
            json_object_set_new(view, "verification_record", history);
            json_object_set_new(view, "previous_volunteers", volunteers);
        } else {
            json_decref(history); json_decref(volunteers);
        }
    }
    json_decref(row);
    if (!ok) { json_decref(view); return NULL; }
    return view;
}

static json_t *wf_project_result(WF_Context *ctx, const char *id) {
    json_t *row = wf_project_row(ctx, id, false);
    return row ? wf_project_view(ctx, row, true) : NULL;
}

static json_t *wf_project_create(WF_Context *ctx, json_t *body) {
    json_t *user = wf_project_actor(ctx, body, true);
    if (!user) return NULL;
    if (!wf_is_patient(user)) {
        json_decref(user);
        return wf_fail(ctx, 403, "only a verified active patient can create a project");
    }
    const char *actor = json_string_value(json_object_get(user, "user_id"));
    const char *bound = json_string_value(json_object_get(user, "project_id"));
    if (json_integer_value(json_object_get(user, "existing_projects")) != 0 || (bound && *bound)) {
        json_decref(user);
        return wf_fail(ctx, 409, "patient already has a project");
    }
    const char *lookup[] = {actor};
    /* The locked patient row serializes creation for this owner. Avoid a gap lock
     * on a missing project, which would make unrelated first-time applicants
     * deadlock when they insert into the same owner-index gap. */
    json_t *existing = wf_query(ctx, "SELECT project_id FROM project WHERE user_id=? AND project_status<>13 LIMIT 1", 1, lookup);
    if (!existing) { json_decref(user); return NULL; }
    if (json_array_size(existing)) {
        json_decref(existing);
        json_decref(user);
        return wf_fail(ctx, 409, "patient already has a project");
    }
    json_decref(existing);
    char id[37], age[16];
    wf_new_id(id);
    snprintf(age, sizeof(age), "%lld", (long long)json_integer_value(json_object_get(user, "user_age")));
    const char *name = json_string_value(json_object_get(user, "user_name"));
    const char *real_name = json_string_value(json_object_get(user, "real_name"));
    if (!name || !real_name || !*real_name) {
        json_decref(user);
        return wf_fail(ctx, 409, "patient profile is incomplete");
    }
    const char *params[] = {id, actor, name, age, real_name};
    bool ok = wf_exec(ctx,
        "INSERT INTO project(project_id,user_id,user_name,user_age,real_name,project_status,"
        "verification_record,current_volunteer,previous_volunteers,question_is_volunteer_assigned,"
        "project_create_time,donation_pool_limit,current_donations_received,current_used_donations,"
        "last_update_time,pathography) VALUES(?,?,?,?,?,1,'[]',NULL,'[]',0,UTC_TIMESTAMP(),0.00,0.00,0.00,UTC_TIMESTAMP(),'[]')",
        5, params);
    const char *bind_params[] = {id, actor};
    if (ok) ok = wf_exec(ctx, "UPDATE user SET existing_projects=1,project_id=? WHERE user_id=?", 2, bind_params);
    json_decref(user);
    return ok ? wf_project_result(ctx, id) : NULL;
}

static json_t *wf_project_submit(WF_Context *ctx, json_t *body) {
    const char *id = wf_project_id(ctx, body);
    if (!id) return NULL;
    json_t *user = wf_project_actor(ctx, body, true);
    if (!user) return NULL;
    if (!wf_is_patient(user)) {
        json_decref(user);
        return wf_fail(ctx, 403, "only a verified active patient can submit a project");
    }
    const char *actor = json_string_value(json_object_get(user, "user_id"));
    json_t *row = wf_project_row(ctx, id, true);
    if (!row) { json_decref(user); return NULL; }
    const char *owner = json_string_value(json_object_get(row, "user_id"));
    if (!owner || strcmp(owner, actor) != 0) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 403, "only the project owner can submit materials");
    }
    int status = (int)json_integer_value(json_object_get(row, "project_status"));
    if (status != 1 && status != 4 && status != 5) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 409, "project is not awaiting submission or correction");
    }
    const char *amount = wf_text(ctx, body, "donation_pool_limit", 11);
    if (!amount || !wf_money(amount)) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 400, "donation_pool_limit must be a positive amount with at most two decimal places");
    }
    json_t *pathography = wf_project_clean_pathography(ctx, json_object_get(body, "pathography"));
    if (!pathography) { json_decref(row); json_decref(user); return NULL; }
    json_t *stored_version = json_object_get(row, "submission_no");
    if (!json_is_integer(stored_version) || json_integer_value(stored_version) < 0 ||
        (uint64_t)json_integer_value(stored_version) >= UINT32_MAX) {
        json_decref(pathography); json_decref(row); json_decref(user);
        return wf_fail(ctx, 409, "Project submission version is unavailable");
    }
    json_int_t submission = json_integer_value(stored_version) + 1;
    char next[4], version[16];
    snprintf(next, sizeof(next), "%d", status == 1 ? 2 : 6);
    snprintf(version, sizeof(version), "%lld", (long long)submission);
    const char *params[] = {next, amount, version, id};
    bool ok = wf_exec(ctx,
        "UPDATE project SET project_status=?,donation_pool_limit=?,submission_no=?,"
        "current_volunteer=NULL,question_is_volunteer_assigned=0,last_update_time=UTC_TIMESTAMP() WHERE project_id=?",
        4, params);
    size_t i;
    json_t *entry;
    json_array_foreach(pathography, i, entry) {
        if (!ok) break;
        ok = wf_files_bind(ctx, json_object_get(entry, "attachments"), actor, "project", id, submission);
        if (ok) ok = wf_project_append_entry(ctx, id, version, entry);
    }
    json_t *snapshot = json_pack("{s:I,s:s,s:O}", "submission_no", submission,
                                 "donation_pool_limit", amount, "pathography", pathography);
    if (ok && !snapshot) { wf_fail(ctx, 500, "Unable to create submission snapshot"); ok = false; }
    if (ok) ok = wf_history_append(ctx, true, id, actor, "submit",
        status == 1 ? "Initial materials submitted" : "Corrected materials resubmitted", snapshot);
    json_decref(snapshot); json_decref(pathography); json_decref(row); json_decref(user);
    return ok ? wf_project_result(ctx, id) : NULL;
}

static json_t *wf_project_get(WF_Context *ctx, json_t *body) {
    const char *id = wf_project_id(ctx, body);
    if (!id) return NULL;
    json_t *user = wf_project_actor(ctx, body, false);
    if (!user) return NULL;
    json_t *row = wf_project_row(ctx, id, false);
    if (!row) { json_decref(user); return NULL; }
    const char *actor = json_string_value(json_object_get(user, "user_id"));
    const char *owner = json_string_value(json_object_get(row, "user_id"));
    const char *volunteer = json_string_value(json_object_get(row, "current_volunteer"));
    bool private_view = owner && strcmp(owner, actor) == 0;
    if (!private_view && wf_is_volunteer(user) && volunteer && strcmp(volunteer, actor) == 0) private_view = true;
    json_decref(user);
    if (!private_view && !wf_project_status_is(row, 0) && !wf_project_status_is(row, 13)) {
        json_decref(row);
        return wf_fail(ctx, 403, "project is not public and actor is not its owner or reviewer");
    }
    return wf_project_view(ctx, row, private_view);
}

static json_t *wf_project_list(WF_Context *ctx, json_t *body) {
    json_t *user = wf_project_actor(ctx, body, false);
    if (!user) return NULL;
    json_decref(user);
    char limit[16], offset[16];
    if (!wf_project_paging(ctx, body, limit, offset)) return NULL;
    json_t *summary = json_object_get(body, "summary");
    if (summary && !json_is_boolean(summary))
        return wf_fail(ctx, 400, "summary must be a boolean");
    const char *params[] = {limit, offset};
    if (json_is_true(summary)) {
        /* Cards only need a bounded preview. Page projects before joining the
         * unique first entry so work and response size never grow with history. */
        return wf_query(ctx,
            "SELECT p.project_id,p.user_name,p.user_age,p.project_status,p.project_create_time,"
            "p.donation_pool_limit,p.current_donations_received,p.current_used_donations,p.last_update_time,"
            "CASE WHEN JSON_TYPE(JSON_EXTRACT(e.record,'$.content'))='STRING' "
            "THEN LEFT(JSON_UNQUOTE(JSON_EXTRACT(e.record,'$.content')),240) ELSE '' END AS summary "
            "FROM (SELECT project_id FROM project WHERE project_status=0 "
            "ORDER BY project_create_time DESC,project_id DESC LIMIT ? OFFSET ?) page "
            "JOIN project p ON p.project_id=page.project_id "
            "LEFT JOIN project_pathography_entry e ON e.project_id=p.project_id "
            "AND e.submission_no=p.submission_no AND e.sequence_no=1 "
            "ORDER BY p.project_create_time DESC,p.project_id DESC", 2, params);
    }
    json_t *rows = wf_query(ctx,
        "SELECT p.project_id,p.user_name,p.user_age,p.project_status,p.project_create_time,p.donation_pool_limit,"
        "p.current_donations_received,p.current_used_donations,p.last_update_time "
        "FROM (SELECT project_id FROM project WHERE project_status=0 "
        "ORDER BY project_create_time DESC,project_id DESC LIMIT ? OFFSET ?) page "
        "JOIN project p ON p.project_id=page.project_id "
        "ORDER BY p.project_create_time DESC,p.project_id DESC", 2, params);
    if (!rows) return NULL;
    /* Resolve the page and current version before reading entries: otherwise
     * the optimizer can scan all historical versions for each project first.
     * Both SELECTs share a transaction and the same deterministic page order. */
    json_t *entries = wf_query(ctx,
        "SELECT e.project_id,e.record FROM "
        "(SELECT project_id FROM project WHERE project_status=0 "
        "ORDER BY project_create_time DESC,project_id DESC LIMIT ? OFFSET ?) page "
        "STRAIGHT_JOIN project p ON p.project_id=page.project_id "
        "STRAIGHT_JOIN project_pathography_entry e ON e.project_id=p.project_id AND e.submission_no=p.submission_no "
        "ORDER BY e.project_id,e.sequence_no", 2, params);
    if (!entries) { json_decref(rows); return NULL; }
    json_t *by_id = json_object();
    size_t i;
    json_t *row;
    json_array_foreach(rows, i, row) {
        json_t *array = json_array();
        json_object_set_new(row, "pathography", array);
        json_object_set(by_id, json_string_value(json_object_get(row, "project_id")), array);
    }
    bool ok = true;
    json_array_foreach(entries, i, row) {
        const char *id = json_string_value(json_object_get(row, "project_id"));
        const char *encoded = json_string_value(json_object_get(row, "record"));
        json_t *entry = encoded ? json_loads(encoded, JSON_DECODE_ANY, NULL) : NULL;
        json_t *target = id ? json_object_get(by_id, id) : NULL;
        if (!entry || !target) {
            json_decref(entry); wf_fail(ctx, 500, "Invalid stored project entry"); ok = false; break;
        }
        if (json_array_append_new(target, entry)) {
            wf_fail(ctx, 500, "Unable to load project entries"); ok = false; break;
        }
    }
    json_decref(entries); json_decref(by_id);
    if (!ok) { json_decref(rows); return NULL; }
    return rows;
}

static json_t *wf_project_tasks(WF_Context *ctx, json_t *body) {
    json_t *user = wf_project_actor(ctx, body, false);
    if (!user) return NULL;
    if (!wf_is_volunteer(user)) {
        json_decref(user);
        return wf_fail(ctx, 403, "only a verified active volunteer can list review tasks");
    }
    char limit[16], offset[16];
    if (!wf_project_paging(ctx, body, limit, offset)) { json_decref(user); return NULL; }
    const char *actor = json_string_value(json_object_get(user, "user_id"));
    /* Every branch uses an equality prefix of the queue/reviewer index. No
     * branch can contribute more than limit+offset rows to the combined page. */
    char window[16];
    snprintf(window, sizeof(window), "%lld", strtoll(limit, NULL, 10) + strtoll(offset, NULL, 10));
    const char *params[] = {actor, window, actor, window, actor, actor, window,
                           actor, actor, window, limit, offset};
    json_t *rows = wf_query(ctx,
        "SELECT * FROM ((SELECT project_id,user_name,user_age,project_status,current_volunteer,"
        "project_create_time,last_update_time FROM project WHERE user_id<>? AND project_status=2 "
        "ORDER BY project_create_time,project_id LIMIT ?) UNION ALL "
        "(SELECT project_id,user_name,user_age,project_status,current_volunteer,project_create_time,last_update_time "
        "FROM project WHERE user_id<>? AND project_status=6 ORDER BY project_create_time,project_id LIMIT ?) UNION ALL "
        "(SELECT project_id,user_name,user_age,project_status,current_volunteer,project_create_time,last_update_time "
        "FROM project WHERE user_id<>? AND project_status=3 AND current_volunteer=? "
        "ORDER BY project_create_time,project_id LIMIT ?) UNION ALL "
        "(SELECT project_id,user_name,user_age,project_status,current_volunteer,project_create_time,last_update_time "
        "FROM project WHERE user_id<>? AND project_status=7 AND current_volunteer=? "
        "ORDER BY project_create_time,project_id LIMIT ?)) tasks "
        "ORDER BY project_create_time,project_id LIMIT ? OFFSET ?", 12, params);
    json_decref(user);
    return rows;
}

static json_t *wf_project_claim(WF_Context *ctx, json_t *body) {
    const char *id = wf_project_id(ctx, body);
    if (!id) return NULL;
    json_t *user = wf_project_actor(ctx, body, true);
    if (!user) return NULL;
    if (!wf_is_volunteer(user)) {
        json_decref(user);
        return wf_fail(ctx, 403, "only a verified active volunteer can claim a task");
    }
    json_t *row = wf_project_row(ctx, id, true);
    if (!row) { json_decref(user); return NULL; }
    const char *actor = json_string_value(json_object_get(user, "user_id"));
    const char *owner = json_string_value(json_object_get(row, "user_id"));
    if (owner && strcmp(owner, actor) == 0) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 403, "applicant cannot review their own project");
    }
    int status = (int)json_integer_value(json_object_get(row, "project_status"));
    if (status != 2 && status != 6) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 409, "task has already been claimed or is not awaiting review");
    }
    char next[4];
    snprintf(next, sizeof(next), "%d", status == 2 ? 3 : 7);
    const char *params[] = {next, actor, id};
    bool ok = wf_exec(ctx,
        "UPDATE project SET project_status=?,current_volunteer=?,question_is_volunteer_assigned=1,"
        "last_update_time=UTC_TIMESTAMP() WHERE project_id=?", 3, params);
    if (ok) ok = wf_history_append(ctx, true, id, actor, "claim", "Volunteer claimed review task", NULL);
    json_decref(row); json_decref(user);
    return ok ? wf_project_result(ctx, id) : NULL;
}

static json_t *wf_project_review(WF_Context *ctx, json_t *body) {
    const char *id = wf_project_id(ctx, body);
    const char *decision = wf_text(ctx, body, "decision", 20);
    const char *note = wf_text(ctx, body, "note", 6000);
    if (!id || !decision || !note) return NULL;
    int next;
    if (strcmp(decision, "approve") == 0) next = 0;
    else if (strcmp(decision, "correction") == 0) next = 5;
    else if (strcmp(decision, "unreachable") == 0) next = 4;
    else return wf_fail(ctx, 400, "decision must be approve, correction or unreachable");
    json_t *user = wf_project_actor(ctx, body, true);
    if (!user) return NULL;
    if (!wf_is_volunteer(user)) {
        json_decref(user);
        return wf_fail(ctx, 403, "only a verified active volunteer can review a project");
    }
    json_t *row = wf_project_row(ctx, id, true);
    if (!row) { json_decref(user); return NULL; }
    const char *actor = json_string_value(json_object_get(user, "user_id"));
    const char *volunteer = json_string_value(json_object_get(row, "current_volunteer"));
    const char *owner = json_string_value(json_object_get(row, "user_id"));
    if (!volunteer || strcmp(volunteer, actor) != 0 || (owner && strcmp(owner, actor) == 0)) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 403, "only the assigned volunteer can review this project");
    }
    int status = (int)json_integer_value(json_object_get(row, "project_status"));
    if (status != 3 && status != 7) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 409, "project is not under review");
    }
    char next_s[4];
    snprintf(next_s, sizeof(next_s), "%d", next);
    const char *params[] = {next_s, next == 0 ? actor : NULL, next == 0 ? "1" : "0", id};
    bool ok = wf_exec(ctx,
        "UPDATE project SET project_status=?,current_volunteer=?,question_is_volunteer_assigned=?,"
        "last_update_time=UTC_TIMESTAMP() WHERE project_id=?", 4, params);
    if (ok) ok = wf_history_append(ctx, true, id, actor, decision, note, NULL);
    json_decref(row); json_decref(user);
    return ok ? wf_project_result(ctx, id) : NULL;
}

static json_t *wf_project_pathography(WF_Context *ctx, json_t *body) {
    const char *id = wf_project_id(ctx, body);
    if (!id) return NULL;
    json_t *user = wf_project_actor(ctx, body, true);
    if (!user) return NULL;
    if (!wf_is_patient(user)) {
        json_decref(user);
        return wf_fail(ctx, 403, "only a verified active patient can update a project");
    }
    json_t *row = wf_project_row(ctx, id, true);
    if (!row) { json_decref(user); return NULL; }
    const char *owner = json_string_value(json_object_get(row, "user_id"));
    const char *actor = json_string_value(json_object_get(user, "user_id"));
    if (!owner || strcmp(owner, actor) != 0) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 403, "only the project owner can append a pathography entry");
    }
    if (!wf_project_status_is(row, 0)) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 409, "only an approved project accepts progress updates");
    }
    json_t *version_value = json_object_get(row, "submission_no");
    if (!json_is_integer(version_value) || json_integer_value(version_value) < 0 ||
        (uint64_t)json_integer_value(version_value) > UINT32_MAX) {
        json_decref(row); json_decref(user);
        return wf_fail(ctx, 500, "Invalid stored project version");
    }
    char version[16];
    snprintf(version, sizeof(version), "%lld", (long long)json_integer_value(version_value));
    json_t *entry = wf_project_clean_entry(ctx, json_object_get(body, "entry"));
    bool ok = entry && wf_files_bind(ctx, json_object_get(entry, "attachments"), actor, "project", id, json_integer_value(version_value));
    if (ok) ok = wf_project_append_entry(ctx, id, version, entry);
    const char *params[] = {id};
    if (ok) ok = wf_exec(ctx, "UPDATE project SET last_update_time=UTC_TIMESTAMP() WHERE project_id=?", 1, params);
    json_decref(entry); json_decref(row); json_decref(user);
    return ok ? wf_project_result(ctx, id) : NULL;
}

#endif
