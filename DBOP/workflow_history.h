#ifndef CROSSDO_WORKFLOW_HISTORY_H
#define CROSSDO_WORKFLOW_HISTORY_H

/* The caller locks the application/user or project parent before appending.
 * Review events grow by rows; historical JSON snapshots are never rewritten. */
static json_t *wf_history_read(WF_Context *ctx, bool project, const char *id) {
    const char *params[] = {id};
    json_t *rows = wf_query(ctx, project
        ? "SELECT record FROM project_review_event WHERE project_id=? ORDER BY sequence_no"
        : "SELECT record FROM application_review_event WHERE user_id=? ORDER BY sequence_no", 1, params);
    if (!rows) return NULL;
    json_t *history = json_array();
    if (!history) { json_decref(rows); return wf_fail(ctx, 500, "Unable to read review history"); }
    size_t index;
    json_t *row;
    json_array_foreach(rows, index, row) {
        const char *stored = json_string_value(json_object_get(row, "record"));
        json_t *record = stored ? json_loads(stored, JSON_DECODE_ANY, NULL) : NULL;
        if (!record) {
            json_decref(rows); json_decref(history);
            return wf_fail(ctx, 500, "Invalid stored review event");
        }
        if (json_array_append_new(history, record)) {
            json_decref(rows); json_decref(history);
            return wf_fail(ctx, 500, "Unable to read review history");
        }
    }
    json_decref(rows);
    return history;
}

static json_t *wf_history_volunteers(WF_Context *ctx, bool project, const char *id,
                                    const char *legacy) {
    json_t *volunteers = wf_json_array(ctx, legacy);
    if (!volunteers) return NULL;
    const char *params[] = {id};
    /* Legacy arrays already include migrated claims. Only append claims created
     * after migration so that a retry/import cannot duplicate old assignments. */
    json_t *rows = wf_query(ctx, project
        ? "SELECT actor_id FROM project_review_event WHERE project_id=? AND decision='claim' AND legacy_import=0 ORDER BY sequence_no"
        : "SELECT actor_id FROM application_review_event WHERE user_id=? AND decision='claim' AND legacy_import=0 ORDER BY sequence_no", 1, params);
    if (!rows) { json_decref(volunteers); return NULL; }
    size_t index;
    json_t *row;
    json_array_foreach(rows, index, row) {
        json_t *actor = json_object_get(row, "actor_id");
        if (!json_is_string(actor) || json_array_append(volunteers, actor)) {
            json_decref(rows); json_decref(volunteers);
            return wf_fail(ctx, 500, "Invalid stored review assignment");
        }
    }
    json_decref(rows);
    return volunteers;
}

static bool wf_history_append(WF_Context *ctx, bool project, const char *id,
                              const char *actor_id, const char *decision,
                              const char *note, json_t *extra) {
    if (extra && !json_is_object(extra)) {
        wf_fail(ctx, 500, "Invalid review event details"); return false;
    }
    static const char *reserved[] = {"actor_id", "decision", "note", "time"};
    for (size_t i = 0; extra && i < sizeof(reserved) / sizeof(reserved[0]); ++i) {
        if (json_object_get(extra, reserved[i])) {
            wf_fail(ctx, 500, "Reserved review event field"); return false;
        }
    }
    char now[20]; wf_now(now);
    json_t *record = json_pack("{s:s,s:s,s:s,s:s}", "actor_id", actor_id,
        "decision", decision, "note", note, "time", now);
    if (!record || (extra && json_object_update(record, extra))) {
        json_decref(record); wf_fail(ctx, 500, "Unable to serialize review event"); return false;
    }
    char *encoded = json_dumps(record, JSON_COMPACT);
    json_decref(record);
    if (!encoded) { wf_fail(ctx, 500, "Unable to serialize review event"); return false; }
    if (strlen(encoded) > 1024 * 1024) {
        free(encoded); wf_fail(ctx, 400, "Review event exceeds storage limit"); return false;
    }
    const char *lookup[] = {id};
    json_t *last = wf_one(ctx, project
        ? "SELECT CAST(COALESCE(MAX(sequence_no),0) AS UNSIGNED) AS sequence_no FROM project_review_event WHERE project_id=?"
        : "SELECT CAST(COALESCE(MAX(sequence_no),0) AS UNSIGNED) AS sequence_no FROM application_review_event WHERE user_id=?", 1, lookup);
    if (!last) { free(encoded); return false; }
    json_t *value = json_object_get(last, "sequence_no");
    json_int_t previous = json_integer_value(value);
    if (!json_is_integer(value) || previous < 0 || previous == LLONG_MAX) {
        json_decref(last); free(encoded);
        wf_fail(ctx, 500, "Invalid review event sequence"); return false;
    }
    char sequence[32]; snprintf(sequence, sizeof(sequence), "%lld", (long long)previous + 1);
    json_decref(last);
    const char *params[] = {id, sequence, actor_id, decision, now, encoded};
    bool ok = wf_exec(ctx, project
        ? "INSERT INTO project_review_event(project_id,sequence_no,actor_id,decision,recorded_at,record,legacy_import) VALUES(?,?,?,?,?,?,0)"
        : "INSERT INTO application_review_event(user_id,sequence_no,actor_id,decision,recorded_at,record,legacy_import) VALUES(?,?,?,?,?,?,0)", 6, params);
    free(encoded);
    bool submitted = !strcmp(decision, "submit"), claimed = !strcmp(decision, "claim");
    bool reviewed = !strcmp(decision, "approve") || !strcmp(decision, "correction") || !strcmp(decision, "unreachable");
    if (ok && (submitted || claimed || reviewed)) {
        json_t *owner = project ? wf_one(ctx, "SELECT user_id FROM project WHERE project_id=?", 1, lookup) : NULL;
        if (project && !owner) return false;
        const char *recipient = project ? json_string_value(json_object_get(owner, "user_id")) : id;
        const char *title = submitted ? "资料已提交" : claimed ? "志愿者已领取审核" :
            !strcmp(decision, "approve") ? "审核已通过" : !strcmp(decision, "correction") ? "请补充审核材料" : "请关注审核联系记录";
        const char *message = submitted ? "我们已收到您的资料，可以在申请页面查看审核进度。" :
            claimed ? "志愿者已开始核验您的资料，请留意后续通知。" : note;
        ok = wf_notify(ctx, recipient, project ? "project_review" : "application_review", title,
                       message, project ? "/my-project" : "/application");
        json_decref(owner);
    }
    return ok;
}

#endif
