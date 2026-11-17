#ifndef CROSSDO_WORKFLOW_OPERATIONS_H
#define CROSSDO_WORKFLOW_OPERATIONS_H

/* Administrative and participant operations run in the router transaction.
 * Existing patient/project parent locks serialize reassignment and closure. */
static const char *wo_id(WF_Context *ctx, json_t *body, const char *key) {
    const char *value = wf_text(ctx, body, key, 36);
    if (value && !wf_uuid(value)) { wf_fail(ctx, 400, "Invalid record identifier"); return NULL; }
    return value;
}

static const char *wo_filter(WF_Context *ctx, json_t *body, const char *key, size_t max) {
    json_t *value = json_object_get(body, key);
    if (!value || json_is_null(value)) return "";
    const char *s = json_string_value(value);
    if (!s || strlen(s) != json_string_length(value) || strlen(s) > max) {
        wf_fail(ctx, 400, "Invalid search filter"); return NULL;
    }
    return s;
}

static bool wo_actor_matches(json_t *actor, const char *id) {
    return id && !strcmp(wa_string(actor, "user_id"), id);
}

static json_t *wo_ok(void) { return json_pack("{s:b}", "ok", 1); }

static json_t *wf_ops_overview(WF_Context *ctx, json_t *body) {
    json_t *actor = wf_require_admin(ctx, body);
    if (!actor) return NULL;
    json_decref(actor);
    return wf_one(ctx, "SELECT "
        "(SELECT COUNT(*) FROM user WHERE user_permission IN(5,7) AND user_status<>0 AND account_state='active') AS pending_volunteers,"
        "(SELECT COUNT(*) FROM user WHERE user_permission=3 AND user_status IN(2,3,6,7)) AS pending_applications,"
        "(SELECT COUNT(*) FROM project WHERE project_status IN(2,3,6,7)) AS pending_projects,"
        "(SELECT COUNT(*) FROM case_report WHERE status='open') AS open_cases,"
        "(SELECT COUNT(*) FROM project_closure WHERE status='pending') AS pending_closures", 0, NULL);
}

static json_t *wf_ops_users(WF_Context *ctx, json_t *body) {
    json_t *actor = wf_require_admin(ctx, body);
    if (!actor) return NULL;
    json_decref(actor);
    char limit[8], offset[16], role[4], pattern[264];
    const char *state = wo_filter(ctx, body, "state", 16), *q = wo_filter(ctx, body, "q", 64);
    if (!state || !q || !wa_page(ctx, body, limit, offset)) return NULL;
    if (*state && strcmp(state, "active") && strcmp(state, "frozen"))
        return wf_fail(ctx, 400, "Unknown account state");
    json_t *r = json_object_get(body, "role");
    int number = r ? (int)json_integer_value(r) : 0;
    if (r && (!json_is_integer(r) || (number!=0 && number!=1 && number!=2 && number!=3 && number!=5 && number!=6 && number!=7)))
        return wf_fail(ctx, 400, "Invalid role filter");
    snprintf(role, sizeof(role), "%d", number);
    snprintf(pattern, sizeof(pattern), "%%%s%%", q);
    const char *params[] = {role,role,state,state,q,pattern,pattern,limit,offset};
    return wf_query(ctx, "SELECT user_id,user_permission,user_name,real_name,user_age,phone_number,"
        "user_status,real_name_authentication,account_state,phone_verified,project_id,existing_projects,registration_date "
        "FROM user WHERE (?='0' OR user_permission=?) AND (?='' OR account_state=?) "
        "AND (?='' OR user_name LIKE ? OR phone_number LIKE ?) "
        "ORDER BY registration_date DESC,user_id DESC LIMIT ? OFFSET ?", 9, params);
}

static json_t *wf_ops_certification(WF_Context *ctx, json_t *body) {
    const char *id = wo_id(ctx, body, "user_id"), *note = wf_text(ctx, body, "note", 4000);
    json_t *approval = json_object_get(body, "approved");
    if (!id || !note) return NULL;
    if (!json_is_boolean(approval)) return wf_fail(ctx, 400, "approved must be a boolean");
    json_t *actor = wf_require_admin(ctx, body);
    if (!actor) return NULL;
    json_t *user = wf_user(ctx, id, true);
    if (!user) { json_decref(actor); return NULL; }
    int role = wa_integer(user, "user_permission");
    if ((role!=5 && role!=7) || strcmp(wa_string(user,"account_state"),"active")) {
        json_decref(actor); json_decref(user);
        return wf_fail(ctx, 409, "Only active volunteer accounts can be certified");
    }
    bool approved = json_is_true(approval);
    const char *params[] = {approved?"0":"5",approved?"1":"0",id};
    bool ok = wf_exec(ctx, "UPDATE user SET user_status=?,real_name_authentication=?,"
        "session_version=session_version+1 WHERE user_id=?", 3, params);
    if (ok) ok = wf_admin_audit(ctx,wa_string(actor,"user_id"),approved?"certify":"certification_reject","user",id,note);
    if (ok) ok = wf_notify(ctx,id,"certification",approved?"志愿者认证已通过":"志愿者认证需要补充核验",note,"/account");
    json_decref(actor); json_decref(user);
    user = ok ? wf_user(ctx,id,false) : NULL;
    json_t *result = user ? wa_public_account(user) : NULL;
    json_decref(user); return result;
}

static json_t *wf_ops_account_state(WF_Context *ctx, json_t *body) {
    const char *id = wo_id(ctx,body,"user_id"), *state = wf_text(ctx,body,"state",16);
    const char *note = wf_text(ctx,body,"note",4000);
    if (!id || !state || !note) return NULL;
    if (strcmp(state,"active") && strcmp(state,"frozen")) return wf_fail(ctx,400,"Unknown account state");
    json_t *actor = wf_require_admin(ctx,body);
    if (!actor) return NULL;
    json_t *user = wf_user(ctx,id,true);
    if (!user) { json_decref(actor); return NULL; }
    if (!strcmp(state,"frozen") && (wo_actor_matches(actor,id) || wa_integer(user,"user_permission")==1)) {
        json_decref(actor); json_decref(user); return wf_fail(ctx,403,"Administrators cannot be frozen through this action");
    }
    bool changed = strcmp(wa_string(user,"account_state"),state)!=0;
    bool ok = true;
    if (changed) {
        const char *params[] = {state,id};
        ok = wf_exec(ctx,"UPDATE user SET account_state=?,session_version=session_version+1 WHERE user_id=?",2,params);
        if (ok) ok = wf_admin_audit(ctx,wa_string(actor,"user_id"),!strcmp(state,"frozen")?"freeze":"unfreeze","user",id,note);
        if (ok) ok = wf_notify(ctx,id,"account_state",!strcmp(state,"frozen")?"账号已暂停使用":"账号已恢复使用",note,"/account");
    }
    json_decref(actor); json_decref(user);
    user = ok ? wf_user(ctx,id,false) : NULL;
    json_t *result = user ? wa_public_account(user) : NULL;
    json_decref(user); return result;
}

static json_t *wf_ops_tasks(WF_Context *ctx, json_t *body) {
    const char *kind = wf_text(ctx,body,"kind",16);
    if (!kind) return NULL;
    bool project = !strcmp(kind,"project");
    if (!project && strcmp(kind,"application")) return wf_fail(ctx,400,"Unknown task kind");
    json_t *actor = wf_require_admin(ctx,body);
    if (!actor) return NULL;
    json_decref(actor);
    char limit[8],offset[16];
    if (!wa_page(ctx,body,limit,offset)) return NULL;
    const char *params[] = {limit,offset};
    return wf_query(ctx,project
        ? "SELECT 'project' AS kind,project_id AS id,user_id,user_name,project_status AS status,current_volunteer,"
          "last_update_time AS updated_at,(current_volunteer IS NOT NULL AND last_update_time<UTC_TIMESTAMP()-INTERVAL 24 HOUR) AS overdue "
          "FROM project WHERE project_status IN(2,3,6,7) ORDER BY last_update_time,project_id LIMIT ? OFFSET ?"
        : "SELECT 'application' AS kind,a.user_id AS id,a.user_id,u.user_name,u.user_status AS status,a.current_volunteer,"
          "a.last_change_time AS updated_at,(a.current_volunteer IS NOT NULL AND a.last_change_time<UTC_TIMESTAMP()-INTERVAL 24 HOUR) AS overdue "
          "FROM patient_application a JOIN user u ON u.user_id=a.user_id WHERE u.user_status IN(2,3,6,7) "
          "ORDER BY a.last_change_time,a.user_id LIMIT ? OFFSET ?",2,params);
}

static json_t *wf_ops_reassign(WF_Context *ctx, json_t *body) {
    const char *kind=wf_text(ctx,body,"kind",16), *id=wo_id(ctx,body,"id"), *note=wf_text(ctx,body,"note",4000);
    if (!kind || !id || !note) return NULL;
    bool project=!strcmp(kind,"project");
    if (!project && strcmp(kind,"application")) return wf_fail(ctx,400,"Unknown task kind");
    json_t *assigned=json_object_get(body,"volunteer_id");
    if (!assigned) return wf_fail(ctx,400,"volunteer_id is required (UUID or null)");
    const char *volunteer=json_is_null(assigned)?NULL:json_string_value(assigned);
    if (!json_is_null(assigned) && (!volunteer || !wf_uuid(volunteer))) return wf_fail(ctx,400,"Invalid volunteer identifier");
    json_t *actor=wf_require_admin(ctx,body);
    if (!actor) return NULL;
    json_t *reviewer=volunteer?wf_user(ctx,volunteer,true):NULL;
    if (volunteer && (!reviewer || !wf_is_volunteer(reviewer))) {
        json_decref(actor); json_decref(reviewer);
        return wf_fail(ctx,409,"Reassignment requires an active verified volunteer");
    }
    json_t *row=project?wf_project_row(ctx,id,true):wf_user(ctx,id,true);
    json_t *app=(!project && row)?wa_application(ctx,id,true):NULL;
    if (!row || (!project && !app)) {
        json_decref(actor); json_decref(reviewer); json_decref(row); json_decref(app); return NULL;
    }
    const char *owner=wa_string(row,"user_id");
    const char *old=wa_string(project?row:app,"current_volunteer");
    int status=wa_integer(row,project?"project_status":"user_status");
    if ((status!=3 && status!=7) || (volunteer && !strcmp(volunteer,owner))) {
        json_decref(actor); json_decref(reviewer); json_decref(row); json_decref(app);
        return wf_fail(ctx,409,"Only assigned reviews may be reassigned; applicants cannot review themselves");
    }
    if (volunteer && !strcmp(old,volunteer)) {
        json_decref(actor); json_decref(reviewer); json_decref(row); json_decref(app);
        return wf_fail(ctx,409,"This volunteer already owns the task");
    }
    char next[4]; snprintf(next,sizeof(next),"%d",volunteer?status:(status==3?2:6));
    bool ok;
    if (project) {
        const char *params[]={next,volunteer,volunteer?"1":"0",id};
        ok=wf_exec(ctx,"UPDATE project SET project_status=?,current_volunteer=?,question_is_volunteer_assigned=?,"
            "last_update_time=UTC_TIMESTAMP() WHERE project_id=?",4,params);
    } else {
        const char *params[]={volunteer,id};
        ok=wf_exec(ctx,"UPDATE patient_application SET current_volunteer=?,last_change_time=UTC_TIMESTAMP() WHERE user_id=?",2,params);
        const char *state[]={next,id};
        if(ok) ok=wf_exec(ctx,"UPDATE user SET user_status=? WHERE user_id=?",2,state);
    }
    json_t *extra=json_pack("{s:s,s:o}","previous_volunteer",old,"volunteer_id",volunteer?json_string(volunteer):json_null());
    if(ok && !extra) { wf_fail(ctx,500,"Unable to record reassignment"); ok=false; }
    if(ok) ok=wf_history_append(ctx,project,id,wa_string(actor,"user_id"),"reassign",note,extra);
    json_decref(extra);
    if(ok && volunteer) ok=wf_history_append(ctx,project,id,volunteer,"claim","Administrator assigned this review",NULL);
    if(ok) ok=wf_admin_audit(ctx,wa_string(actor,"user_id"),"reassign",kind,id,note);
    char link[100]; snprintf(link,sizeof(link),"/review/%s/%s",kind,id);
    if(ok && *old) ok=wf_notify(ctx,old,"reassignment","审核任务已转派",note,"/tasks");
    if(ok && volunteer) ok=wf_notify(ctx,volunteer,"assignment","您收到一项审核任务",note,link);
    if(ok) ok=wf_notify(ctx,owner,"reassignment","申请审核负责人已调整",note,project?"/my-project":"/application");
    json_decref(actor); json_decref(reviewer); json_decref(row); json_decref(app);
    return ok?wo_ok():NULL;
}

static json_t *wf_ops_notifications(WF_Context *ctx,json_t *body) {
    const char *actor=wf_actor_id(ctx,body); char limit[8],offset[16];
    if(!actor || !wa_page(ctx,body,limit,offset)) return NULL;
    const char *params[]={actor,limit,offset};
    return wf_query(ctx,"SELECT id,type,title,body,link,created_at,read_at FROM notification "
        "WHERE user_id=? ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?",3,params);
}
static json_t *wf_ops_unread(WF_Context *ctx,json_t *body) {
    const char *actor=wf_actor_id(ctx,body); if(!actor) return NULL;
    const char *params[]={actor};
    return wf_one(ctx,"SELECT COUNT(*) AS count FROM notification WHERE user_id=? AND read_at IS NULL",1,params);
}
static json_t *wf_ops_notification_read(WF_Context *ctx,json_t *body) {
    const char *actor=wf_actor_id(ctx,body),*id=wo_id(ctx,body,"id"); if(!actor||!id) return NULL;
    const char *params[]={id,actor};
    json_t *row=wf_one(ctx,"SELECT id FROM notification WHERE id=? AND user_id=? FOR UPDATE",2,params);
    if(!row) return NULL;
    json_decref(row);
    return wf_exec(ctx,"UPDATE notification SET read_at=COALESCE(read_at,UTC_TIMESTAMP()) WHERE id=? AND user_id=?",2,params)?wo_ok():NULL;
}
static json_t *wf_ops_notifications_read_all(WF_Context *ctx,json_t *body) {
    const char *actor=wf_actor_id(ctx,body); if(!actor) return NULL;
    const char *params[]={actor};
    return wf_exec(ctx,"UPDATE notification SET read_at=UTC_TIMESTAMP() WHERE user_id=? AND read_at IS NULL",1,params)?wo_ok():NULL;
}

static json_t *wo_case_row(WF_Context *ctx,const char *id,bool lock) {
    const char *params[]={id};
    return wf_one(ctx,lock?
        "SELECT c.*,p.user_id AS project_owner_id FROM case_report c LEFT JOIN project p ON p.project_id=c.project_id WHERE c.id=? FOR UPDATE":
        "SELECT c.*,p.user_id AS project_owner_id FROM case_report c LEFT JOIN project p ON p.project_id=c.project_id WHERE c.id=?",1,params);
}
static bool wo_case_related(json_t *actor,json_t *row) {
    const char *id=wa_string(actor,"user_id");
    return wf_admin_user(actor)||!strcmp(id,wa_string(row,"reporter_id"))||
        !strcmp(id,wa_string(row,"target_user_id"))||!strcmp(id,wa_string(row,"project_owner_id"));
}
static bool wo_case_event(WF_Context *ctx,const char *id,const char *actor,const char *action,const char *note) {
    const char *params[]={id,actor,action,note};
    return wf_exec(ctx,"INSERT INTO case_event(case_id,actor_id,action,note,created_at) VALUES(?,?,?,?,UTC_TIMESTAMP())",4,params);
}
static json_t *wo_case_decode(WF_Context *ctx,json_t *row,bool events) {
    json_t *evidence=wf_json_array(ctx,wa_string(row,"evidence"));
    if(!evidence) { json_decref(row); return NULL; }
    json_object_set_new(row,"evidence",evidence);
    json_object_del(row,"project_owner_id");
    if(events) {
        const char *params[]={wa_string(row,"id")};
        json_t *history=wf_query(ctx,"SELECT actor_id,action,note,created_at FROM case_event WHERE case_id=? ORDER BY id",1,params);
        if(!history) { json_decref(row); return NULL; }
        json_object_set_new(row,"events",history);
    }
    return row;
}
static bool wo_case_notify(WF_Context *ctx,json_t *row,const char *title,const char *note) {
    char link[80]; snprintf(link,sizeof(link),"/cases?id=%s",wa_string(row,"id"));
    const char *reporter=wa_string(row,"reporter_id");
    const char *target=*wa_string(row,"target_user_id")?wa_string(row,"target_user_id"):wa_string(row,"project_owner_id");
    if(!wf_notify(ctx,reporter,"case",title,note,link)) return false;
    return !*target||!strcmp(target,reporter)||wf_notify(ctx,target,"case",title,note,link);
}
static json_t *wf_ops_case_create(WF_Context *ctx,json_t *body) {
    const char *kind=wf_text(ctx,body,"kind",16),*reason=wf_text(ctx,body,"reason",6000);
    json_t *refs=json_object_get(body,"evidence");
    if(!kind||!reason) return NULL;
    bool appeal=!strcmp(kind,"appeal");
    if(!appeal&&strcmp(kind,"report")) return wf_fail(ctx,400,"Unknown case kind");
    if(!json_is_array(refs)||json_array_size(refs)>20) return wf_fail(ctx,400,"Evidence must be an array of at most 20 links");
    json_t *actor=wa_actor_checked(ctx,body,false,false); if(!actor) return NULL;
    if(!wf_files_owned_refs(ctx,refs,wa_string(actor,"user_id"))) { json_decref(actor); return NULL; }
    const char *project=wo_filter(ctx,body,"project_id",36),*target=wo_filter(ctx,body,"target_user_id",36),*parent=wo_filter(ctx,body,"parent_id",36);
    if(!project||!target||!parent) { json_decref(actor); return NULL; }
    json_t *prior=NULL,*subject=NULL;
    if(appeal) {
        if(!wf_uuid(parent)) { json_decref(actor); return wf_fail(ctx,400,"Appeal requires a parent case"); }
        prior=wo_case_row(ctx,parent,true);
        if(!prior) { json_decref(actor); return NULL; }
        if(!wo_case_related(actor,prior)||!strcmp(wa_string(prior,"status"),"open")) {
            json_decref(actor); json_decref(prior); return wf_fail(ctx,403,"Only participants may appeal a decided case");
        }
        project=wa_string(prior,"project_id"); target=wa_string(prior,"target_user_id");
        const char *params[]={parent,wa_string(actor,"user_id")};
        json_t *duplicates=wf_query(ctx,"SELECT id FROM case_report WHERE parent_id=? AND reporter_id=? AND status='open' LIMIT 1 FOR UPDATE",2,params);
        if(!duplicates) { json_decref(actor); json_decref(prior); return NULL; }
        bool duplicate=json_array_size(duplicates)>0; json_decref(duplicates);
        if(duplicate) { json_decref(actor); json_decref(prior); return wf_fail(ctx,409,"An open appeal already exists"); }
    } else if(*parent) { json_decref(actor); return wf_fail(ctx,400,"A report cannot specify a parent"); }
    if((!*project==!*target)||(*project&&!wf_uuid(project))||(*target&&!wf_uuid(target))) {
        json_decref(actor); json_decref(prior); return wf_fail(ctx,400,"Exactly one valid case target is required");
    }
    if(*project) {
        subject=wf_project_row(ctx,project,false);
        if(subject&&!appeal&&!wf_admin_user(actor)&&!wo_actor_matches(actor,wa_string(subject,"user_id"))&&
            !wf_project_status_is(subject,0)&&!wf_project_status_is(subject,13)) {
            json_decref(subject); subject=NULL; wf_fail(ctx,403,"Project is not available for reporting");
        }
    } else subject=wf_user(ctx,target,false);
    if(!subject) { json_decref(actor); json_decref(prior); return NULL; }
    char id[37]; wf_new_id(id);
    char *evidence=wf_dump(ctx,refs);
    const char *params[]={id,wa_string(actor,"user_id"),*target?target:NULL,*project?project:NULL,appeal?parent:NULL,kind,reason,evidence};
    bool ok=evidence&&wf_exec(ctx,"INSERT INTO case_report(id,reporter_id,target_user_id,project_id,parent_id,kind,reason,evidence,status,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,'open',UTC_TIMESTAMP(),UTC_TIMESTAMP())",8,params);
    if(ok) ok=wo_case_event(ctx,id,wa_string(actor,"user_id"),"created",reason);
    free(evidence); json_decref(actor); json_decref(prior); json_decref(subject);
    json_t *row=ok?wo_case_row(ctx,id,false):NULL;
    if(row&&!wo_case_notify(ctx,row,"举报或申诉已受理","运营人员将核查相关记录，处理结果会在此通知。")) { json_decref(row); return NULL; }
    return row?wo_case_decode(ctx,row,true):NULL;
}
static json_t *wo_cases(WF_Context *ctx,json_t *body,bool admin) {
    json_t *actor=admin?wf_require_admin(ctx,body):wa_actor_checked(ctx,body,false,false);
    if(!actor) return NULL;
    char limit[8],offset[16]; const char *status=wo_filter(ctx,body,"status",16);
    if(!status||!wa_page(ctx,body,limit,offset)) { json_decref(actor); return NULL; }
    if(*status&&strcmp(status,"open")&&strcmp(status,"resolved")&&strcmp(status,"dismissed")) {
        json_decref(actor); return wf_fail(ctx,400,"Unknown case status");
    }
    const char *id=wa_string(actor,"user_id");
    const char *params[]={admin?"1":"0",id,id,id,status,status,limit,offset};
    json_t *rows=wf_query(ctx,"SELECT c.* FROM case_report c LEFT JOIN project p ON p.project_id=c.project_id "
        "WHERE (?='1' OR c.reporter_id=? OR c.target_user_id=? OR p.user_id=?) AND (?='' OR c.status=?) "
        "ORDER BY c.created_at DESC,c.id DESC LIMIT ? OFFSET ?",8,params);
    if(!rows) { json_decref(actor); return NULL; }
    size_t index; json_t *row;
    json_array_foreach(rows,index,row) {
        bool visible=wf_admin_user(actor)||!strcmp(id,wa_string(row,"reporter_id"));
        json_t *refs=visible?wf_json_array(ctx,wa_string(row,"evidence")):json_array();
        if(!refs) { json_decref(actor); json_decref(rows); return NULL; }
        json_object_set_new(row,"evidence",refs);
        json_object_set_new(row,"evidence_private",json_boolean(!visible));
    }
    json_decref(actor);
    return rows;
}
static json_t *wf_ops_cases(WF_Context *ctx,json_t *body) { return wo_cases(ctx,body,false); }
static json_t *wf_ops_admin_cases(WF_Context *ctx,json_t *body) { return wo_cases(ctx,body,true); }
static json_t *wf_ops_case_get(WF_Context *ctx,json_t *body) {
    const char *id=wo_id(ctx,body,"id"); if(!id) return NULL;
    json_t *actor=wa_actor_checked(ctx,body,false,false); if(!actor) return NULL;
    json_t *row=wo_case_row(ctx,id,false); if(!row) { json_decref(actor); return NULL; }
    bool allowed=wo_case_related(actor,row);
    bool evidence_visible=wf_admin_user(actor)||wo_actor_matches(actor,wa_string(row,"reporter_id"));
    json_decref(actor);
    if(!allowed) { json_decref(row); return wf_fail(ctx,403,"Case is restricted to its participants"); }
    if(!evidence_visible) json_object_set_new(row,"evidence",json_string("[]"));
    json_object_set_new(row,"evidence_private",json_boolean(!evidence_visible));
    return wo_case_decode(ctx,row,true);
}
static json_t *wf_ops_case_resolve(WF_Context *ctx,json_t *body) {
    const char *id=wo_id(ctx,body,"id"),*decision=wf_text(ctx,body,"decision",16),*note=wf_text(ctx,body,"note",6000);
    if(!id||!decision||!note) return NULL;
    if(strcmp(decision,"resolved")&&strcmp(decision,"dismissed")) return wf_fail(ctx,400,"Unknown case decision");
    json_t *actor=wf_require_admin(ctx,body); if(!actor) return NULL;
    json_t *row=wo_case_row(ctx,id,true); if(!row) { json_decref(actor); return NULL; }
    if(strcmp(wa_string(row,"status"),"open")) {
        json_decref(actor); json_decref(row); return wf_fail(ctx,409,"Case is already decided");
    }
    const char *params[]={decision,note,wa_string(actor,"user_id"),id};
    bool ok=wf_exec(ctx,"UPDATE case_report SET status=?,decision_note=?,resolved_by=?,updated_at=UTC_TIMESTAMP(),resolved_at=UTC_TIMESTAMP() WHERE id=?",4,params);
    if(ok) ok=wo_case_event(ctx,id,wa_string(actor,"user_id"),decision,note);
    if(ok) ok=wf_admin_audit(ctx,wa_string(actor,"user_id"),"case_resolve","case",id,note);
    if(ok) ok=wo_case_notify(ctx,row,"举报或申诉已有处理结果",note);
    json_decref(actor); json_decref(row);
    row=ok?wo_case_row(ctx,id,false):NULL;
    return row?wo_case_decode(ctx,row,true):NULL;
}

static json_t *wo_closure_row(WF_Context *ctx,const char *id) {
    const char *params[]={id};
    return wf_one(ctx,"SELECT c.*,p.user_name,p.project_status FROM project_closure c JOIN project p ON p.project_id=c.project_id WHERE c.project_id=?",1,params);
}
static json_t *wf_ops_closure_request(WF_Context *ctx,json_t *body) {
    const char *id=wo_id(ctx,body,"project_id"),*reason=wf_text(ctx,body,"reason",6000),*plan=wf_text(ctx,body,"remaining_plan",16);
    if(!id||!reason||!plan) return NULL;
    if(strcmp(plan,"refund")&&strcmp(plan,"fully_used")) return wf_fail(ctx,400,"Unknown remaining-funds plan");
    json_t *actor=wa_actor_checked(ctx,body,false,false); if(!actor) return NULL;
    json_t *project=wf_project_row(ctx,id,true); if(!project) { json_decref(actor); return NULL; }
    if(!wf_admin_user(actor)&&!wo_actor_matches(actor,wa_string(project,"user_id"))) {
        json_decref(actor); json_decref(project); return wf_fail(ctx,403,"Only the owner may request closure");
    }
    if(!wf_project_status_is(project,0)) {
        json_decref(actor); json_decref(project); return wf_fail(ctx,409,"Only an approved project may request closure");
    }
    const char *params[]={id,wa_string(actor,"user_id"),reason,plan};
    bool ok=wf_exec(ctx,"INSERT INTO project_closure(project_id,requester_id,reason,remaining_plan,status,requested_at) "
        "VALUES(?,?,?,?,'pending',UTC_TIMESTAMP()) ON DUPLICATE KEY UPDATE requester_id=VALUES(requester_id),reason=VALUES(reason),"
        "remaining_plan=VALUES(remaining_plan),status='pending',requested_at=UTC_TIMESTAMP(),decision_note=NULL,reviewed_by=NULL,reviewed_at=NULL",4,params);
    const char *target[]={id};
    if(ok) ok=wf_exec(ctx,"UPDATE project SET project_status=12,last_update_time=UTC_TIMESTAMP() WHERE project_id=?",1,target);
    json_t *extra=json_pack("{s:s}","remaining_plan",plan);
    if(ok&&extra) ok=wf_history_append(ctx,true,id,wa_string(actor,"user_id"),"closure_request",reason,extra);
    else if(ok) { wf_fail(ctx,500,"Unable to record closure"); ok=false; }
    if(ok) ok=wf_notify(ctx,wa_string(project,"user_id"),"closure","项目结项申请已提交","筹款已暂停；运营人员将在资金结清后审核结项。","/my-project");
    json_decref(extra); json_decref(actor); json_decref(project);
    return ok?wo_closure_row(ctx,id):NULL;
}
static json_t *wf_ops_closure_get(WF_Context *ctx,json_t *body) {
    const char *id=wo_id(ctx,body,"project_id"); if(!id) return NULL;
    bool authenticated=json_object_get(body,"actor_id")!=NULL;
    json_t *actor=authenticated?wa_actor_checked(ctx,body,false,false):NULL;
    if(authenticated&&!actor) return NULL;
    json_t *project=wf_project_row(ctx,id,false); if(!project) { json_decref(actor); return NULL; }
    bool private_view=actor&&(wf_admin_user(actor)||wo_actor_matches(actor,wa_string(project,"user_id")));
    bool allowed=private_view||wf_project_status_is(project,13);
    json_decref(actor); json_decref(project);
    if(!allowed) return wf_fail(ctx,403,"Closure is not public before approval");
    json_t *row=wo_closure_row(ctx,id);
    if(row&&!private_view) {
        json_object_del(row,"requester_id");
        json_object_del(row,"reviewed_by");
        json_object_del(row,"decision_note");
    }
    return row;
}
static json_t *wf_ops_closures(WF_Context *ctx,json_t *body) {
    json_t *actor=wf_require_admin(ctx,body); if(!actor) return NULL;
    json_decref(actor);
    char limit[8],offset[16]; if(!wa_page(ctx,body,limit,offset)) return NULL;
    const char *params[]={limit,offset};
    return wf_query(ctx,"SELECT c.*,p.user_name,p.project_status FROM project_closure c JOIN project p ON p.project_id=c.project_id "
        "ORDER BY (c.status='pending') DESC,c.requested_at,c.project_id LIMIT ? OFFSET ?",2,params);
}

/* Every read here must be a locking/current read: the router may already have
 * opened an older REPEATABLE READ snapshot before acquiring the project lock.
 * Finance mutations also acquire that project lock, so the current-read results
 * remain stable until this transaction approves or rejects closure. */
static bool wo_closure_settled(WF_Context *ctx,json_t *project) {
    const char *params[]={wa_string(project,"project_id")};
    json_t *account=wf_query(ctx,"SELECT balance=0 AND reserved=0 AS settled FROM project_fund_account WHERE project_id=? FOR UPDATE",1,params);
    if(!account) return false;
    bool settled=!json_array_size(account)||wa_integer(json_array_get(account,0),"settled")==1;
    json_decref(account);
    if(!settled) goto unsettled;
    json_t *pending=wf_query(ctx,"SELECT id FROM payment_order WHERE project_id=? AND status='pending' LIMIT 1 FOR UPDATE",1,params);
    if(!pending) return false;
    settled=!json_array_size(pending); json_decref(pending);
    if(!settled) goto unsettled;
    const char *refund_params[]={wa_string(project,"current_donations_received"),wa_string(project,"current_used_donations"),params[0]};
    json_t *refunds=wf_one(ctx,"SELECT COUNT(IF(status IN('pending','processing'),1,NULL))=0 "
        "AND CAST(? AS DECIMAL(14,2))-CAST(? AS DECIMAL(14,2)) "
        "-COALESCE(SUM(IF(status='succeeded',amount,0)),0)=0 AS settled "
        "FROM refund_request WHERE project_id=? FOR UPDATE",3,refund_params);
    if(!refunds) return false;
    settled=wa_integer(refunds,"settled")==1; json_decref(refunds);
    if(!settled) goto unsettled;
    pending=wf_query(ctx,"SELECT id FROM fund_disbursement WHERE project_id=? "
        "AND status IN('requested','approved','processing') LIMIT 1 FOR UPDATE",1,params);
    if(!pending) return false;
    settled=!json_array_size(pending); json_decref(pending);
    if(!settled) goto unsettled;
    json_t *ledger=wf_one(ctx,"SELECT COALESCE(SUM(amount),0)=0 AS settled FROM fund_ledger WHERE project_id=? FOR UPDATE",1,params);
    if(!ledger) return false;
    settled=wa_integer(ledger,"settled")==1; json_decref(ledger);
    if(settled) return true;
unsettled:
    wf_fail(ctx,409,"Settle pending payments, refunds and remaining funds before closing the project");
    return false;
}

static json_t *wf_ops_closure_review(WF_Context *ctx,json_t *body) {
    const char *id=wo_id(ctx,body,"project_id"),*decision=wf_text(ctx,body,"decision",16),*note=wf_text(ctx,body,"note",6000);
    if(!id||!decision||!note) return NULL;
    bool approve=!strcmp(decision,"approve");
    if(!approve&&strcmp(decision,"reject")) return wf_fail(ctx,400,"Unknown closure decision");
    json_t *actor=wf_require_admin(ctx,body); if(!actor) return NULL;
    /* Project is the serialization point shared with finance callbacks. */
    json_t *project=wf_project_row(ctx,id,true);
    if(!project) { json_decref(actor); return NULL; }
    json_t *closure=wo_closure_row(ctx,id);
    if(!closure) { json_decref(actor); json_decref(project); return NULL; }
    if(!wf_project_status_is(project,12)||strcmp(wa_string(closure,"status"),"pending")) {
        json_decref(actor); json_decref(project); json_decref(closure); return wf_fail(ctx,409,"Closure is not awaiting review");
    }
    bool ok=!approve||wo_closure_settled(ctx,project);
    const char *params[]={approve?"approved":"rejected",note,wa_string(actor,"user_id"),id};
    if(ok) ok=wf_exec(ctx,"UPDATE project_closure SET status=?,decision_note=?,reviewed_by=?,reviewed_at=UTC_TIMESTAMP() WHERE project_id=?",4,params);
    const char *state[]={approve?"13":"0",id};
    if(ok) ok=wf_exec(ctx,"UPDATE project SET project_status=?,last_update_time=UTC_TIMESTAMP() WHERE project_id=?",2,state);
    if(ok&&approve) {
        const char *owner[]={wa_string(project,"user_id"),id};
        ok=wf_exec(ctx,"UPDATE user SET project_id=NULL,existing_projects=0 WHERE user_id=? AND project_id=?",2,owner);
    }
    if(ok) ok=wf_history_append(ctx,true,id,wa_string(actor,"user_id"),approve?"closure_approved":"closure_rejected",note,NULL);
    if(ok) ok=wf_admin_audit(ctx,wa_string(actor,"user_id"),"closure_review","project",id,note);
    if(ok) ok=wf_notify(ctx,wa_string(project,"user_id"),"closure",approve?"项目已完成结项":"结项申请未通过",note,"/projects");
    json_decref(actor); json_decref(project); json_decref(closure);
    return ok?wo_closure_row(ctx,id):NULL;
}
#endif
