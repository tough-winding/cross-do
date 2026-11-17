#ifndef CROSSDO_WORKFLOW_FINANCE_H
#define CROSSDO_WORKFLOW_FINANCE_H

/* The project lock serializes every monetary mutation and closure decision.
 * Balance, request state, idempotency and event checks use locking/current
 * reads: actor lookup may already have established a REPEATABLE READ snapshot.
 * Java performs network calls only after the database transaction commits. */
static const char *fin_s(json_t *o, const char *k) {
    const char *s=json_string_value(json_object_get(o,k)); return s?s:"";
}
static bool fin_admin(json_t *u) { return wf_admin_user(u); }
static json_t *fin_actor(WF_Context *ctx,json_t *body,bool admin) {
    const char *id=wf_actor_id(ctx,body); if(!id)return NULL;
    json_t *u=wf_user(ctx,id,false); if(!u)return NULL;
    if(strcmp(fin_s(u,"account_state"),"active") || json_integer_value(json_object_get(u,"user_status"))>=8 ||
       (admin && (!fin_admin(u) || json_integer_value(json_object_get(u,"user_status"))!=0))) {
        json_decref(u); return wf_fail(ctx,403,"An active authorized account is required");
    }
    return u;
}
static const char *fin_id(WF_Context *ctx,json_t *b,const char *k) {
    const char *id=wf_text(ctx,b,k,36);
    if(id&&!wf_uuid(id)){wf_fail(ctx,400,"Invalid financial identifier");return NULL;} return id;
}
static const char *fin_amount(WF_Context *ctx,json_t *b) {
    const char *a=wf_text(ctx,b,"amount",11);
    if(a&&!wf_money(a)){wf_fail(ctx,400,"Amount must be positive with at most two decimal places");return NULL;}return a;
}
static const char *fin_mode(WF_Context *ctx,json_t *b) {
    const char *m=wf_text(ctx,b,"mode",16);
    if(m&&strcmp(m,"development")&&strcmp(m,"gateway")){wf_fail(ctx,400,"Invalid payment mode");return NULL;}return m;
}
/* Decimal-to-cents conversion is exact; never use floating point for money. */
static int64_t fin_cents(const char *s) {
    int64_t n=0;int frac=0;
    if(!s||!*s)return 0;
    for(;*s&&*s!='.';s++){if(*s<'0'||*s>'9')return -1;n=n*10+(*s-'0');if(n>999999999999LL)return -1;}
    n*=100;
    if(*s=='.'){s++;if(*s){if(*s<'0'||*s>'9')return -1;frac=(*s++-'0')*10;}
        if(*s){if(*s<'0'||*s>'9')return -1;frac+=*s++-'0';}if(*s)return -1;}
    return n+frac;
}
static bool fin_equal_amount(const char *a,const char *b){return fin_cents(a)==fin_cents(b)&&fin_cents(a)>0;}
static json_t *fin_find(WF_Context *ctx,const char *sql,size_t n,const char *const *p) {
    json_t *rows=wf_query(ctx,sql,n,p);if(!rows)return NULL;
    json_t *one=json_array_size(rows)?json_incref(json_array_get(rows,0)):json_null();
    json_decref(rows);return one;
}
static json_t *fin_account(WF_Context *ctx,const char *pid,const char *mode,bool create) {
    const char *args[]={pid,mode};
    if(create&&!wf_exec(ctx,"INSERT INTO project_fund_account(project_id,mode,balance,reserved,updated_at) "
        "VALUES(?,?,0,0,UTC_TIMESTAMP()) ON DUPLICATE KEY UPDATE project_id=VALUES(project_id)",2,args))return NULL;
    json_t *a=fin_find(ctx,"SELECT * FROM project_fund_account WHERE project_id=? FOR UPDATE",1,args);
    if(a&&!json_is_null(a)&&mode&&strcmp(fin_s(a,"mode"),mode)){
        json_decref(a);return wf_fail(ctx,409,"Development and gateway funds cannot be mixed in a project");
    }return a;
}
static json_t *fin_row(WF_Context *ctx,const char *kind,const char *id,bool lock) {
    const char *p[]={id}; const char *sql=NULL;
    if(!strcmp(kind,"payment"))sql=lock?"SELECT * FROM payment_order WHERE id=? FOR UPDATE":"SELECT * FROM payment_order WHERE id=?";
    else if(!strcmp(kind,"refund"))sql=lock?"SELECT * FROM refund_request WHERE id=? FOR UPDATE":"SELECT * FROM refund_request WHERE id=?";
    else sql=lock?"SELECT * FROM fund_disbursement WHERE id=? FOR UPDATE":"SELECT * FROM fund_disbursement WHERE id=?";
    return wf_one(ctx,sql,1,p);
}
static json_t *fin_decode_evidence(WF_Context *ctx,json_t *row) {
    if(!row)return NULL;
    json_t *v=json_object_get(row,"evidence");
    if(v&&json_is_string(v)){
        json_t *a=wf_json_array(ctx,json_string_value(v));
        if(!a){json_decref(row);return NULL;}json_object_set_new(row,"evidence",a);
    }return row;
}
static json_t *fin_order_view(WF_Context *ctx,json_t *order) {
    if(!order)return NULL;
    const char *p[]={fin_s(order,"id")};
    json_t *refunds=wf_query(ctx,"SELECT id,order_id,project_id,requester_id,amount,reason,status,mode,created_at,updated_at,completed_at "
        "FROM refund_request WHERE order_id=? ORDER BY created_at,id",1,p);
    if(!refunds){json_decref(order);return NULL;}
    json_object_set_new(order,"refunds",refunds);json_object_del(order,"idempotency_key");return order;
}
static json_t *fin_order_get(WF_Context *ctx,json_t *body){
    const char *id=fin_id(ctx,body,"id");if(!id)return NULL;
    json_t *u=fin_actor(ctx,body,false);if(!u)return NULL;
    json_t *o=fin_row(ctx,"payment",id,false);
    if(o&&!fin_admin(u)&&strcmp(fin_s(o,"donor_id"),fin_s(u,"user_id"))){
        json_decref(o);o=wf_fail(ctx,403,"Only the donor or administrator may read an order");
    }json_decref(u);return fin_order_view(ctx,o);
}
static json_t *fin_orders_list(WF_Context *ctx,json_t *body) {
    bool admin=json_is_true(json_object_get(body,"admin"));
    json_t *u=fin_actor(ctx,body,admin);if(!u)return NULL;
    char limit[8],offset[16];if(!wa_page(ctx,body,limit,offset)){json_decref(u);return NULL;}
    const char *pid=json_string_value(json_object_get(body,"project_id"));
    if(pid&&!wf_uuid(pid)){json_decref(u);return wf_fail(ctx,400,"Invalid project identifier");}
    const char *p[]={admin?pid:fin_s(u,"user_id"),limit,offset};
    json_t *rows=admin?(pid?wf_query(ctx,"SELECT * FROM payment_order WHERE project_id=? ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?",3,p):
        wf_query(ctx,"SELECT * FROM payment_order ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?",2,p+1)):
        wf_query(ctx,"SELECT * FROM payment_order WHERE donor_id=? ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?",3,p);
    json_decref(u);if(!rows)return NULL;
    /* Batch refund children for this bounded page; avoid one query per order. */
    size_t size=json_array_size(rows);
    if(size){
        char sql[1500]="SELECT id,order_id,project_id,requester_id,amount,reason,status,mode,created_at,updated_at,completed_at FROM refund_request WHERE order_id IN (";
        const char *ids[100];size_t i;json_t *row;
        json_array_foreach(rows,i,row){ids[i]=fin_s(row,"id");strcat(sql,i?",?":"?");}
        strcat(sql,") ORDER BY created_at,id");
        json_t *children=wf_query(ctx,sql,size,ids);
        if(!children){json_decref(rows);return NULL;}
        json_array_foreach(rows,i,row){
            json_t *a=json_array();size_t j;json_t *child;
            json_array_foreach(children,j,child)if(!strcmp(fin_s(child,"order_id"),fin_s(row,"id")))json_array_append(a,child);
            json_object_set_new(row,"refunds",a);json_object_del(row,"idempotency_key");
        }json_decref(children);
    }return rows;
}
static json_t *fin_order_create(WF_Context *ctx,json_t *body) {
    const char *pid=fin_id(ctx,body,"project_id"),*key=fin_id(ctx,body,"idempotency_key");
    const char *amount=fin_amount(ctx,body),*mode=fin_mode(ctx,body);
    if(!pid||!key||!amount||!mode)return NULL;
    json_t *u=fin_actor(ctx,body,false);if(!u)return NULL;
    json_t *project=wf_project_row(ctx,pid,true);if(!project){json_decref(u);return NULL;}
    const char *uid=fin_s(u,"user_id");const char *lookup[]={uid,key};
    json_t *existing=fin_find(ctx,"SELECT * FROM payment_order WHERE donor_id=? AND idempotency_key=? FOR UPDATE",2,lookup);
    if(!existing){json_decref(u);json_decref(project);return NULL;}
    if(!json_is_null(existing)){
        bool same=!strcmp(fin_s(existing,"project_id"),pid)&&fin_equal_amount(fin_s(existing,"amount"),amount)&&!strcmp(fin_s(existing,"mode"),mode);
        json_decref(u);json_decref(project);
        if(!same){json_decref(existing);return wf_fail(ctx,409,"Idempotency key was used with different payment details");}
        return fin_order_view(ctx,existing);
    }json_decref(existing);
    if(!wf_project_status_is(project,0)||!strcmp(fin_s(project,"user_id"),uid)){
        json_decref(u);json_decref(project);return wf_fail(ctx,409,"Only open approved projects accept donations from other users");
    }
    json_t *account=fin_account(ctx,pid,mode,true);
    bool ok=account!=NULL;json_decref(account);
    char id[37];wf_new_id(id);const char *p[]={id,pid,uid,amount,mode,key};
    if(ok)ok=wf_exec(ctx,"INSERT INTO payment_order(id,project_id,donor_id,amount,status,mode,idempotency_key,created_at,updated_at) "
        "VALUES(?,?,?,?,'pending',?,?,UTC_TIMESTAMP(),UTC_TIMESTAMP())",6,p);
    json_decref(u);json_decref(project);return ok?fin_order_view(ctx,fin_row(ctx,"payment",id,false)):NULL;
}
static json_t *fin_order_checkout(WF_Context *ctx,json_t *body){
    const char *id=fin_id(ctx,body,"id"),*url=wf_text(ctx,body,"checkout_url",2048);
    if(!id||!url)return NULL;
    if(strncmp(url,"https://",8))return wf_fail(ctx,400,"Checkout URL must use HTTPS");
    json_t *u=fin_actor(ctx,body,false);if(!u)return NULL;
    json_t *r=fin_row(ctx,"payment",id,false);if(!r){json_decref(u);return NULL;}
    if(strcmp(fin_s(r,"donor_id"),fin_s(u,"user_id"))){json_decref(r);json_decref(u);return wf_fail(ctx,403,"Only the donor can prepare checkout");}
    const char *p[]={url,id};
    bool ok=wf_exec(ctx,"UPDATE payment_order SET checkout_url=?,updated_at=UTC_TIMESTAMP() WHERE id=? AND mode='gateway' AND status='pending'",2,p);
    json_decref(r);json_decref(u);return ok?fin_order_view(ctx,fin_row(ctx,"payment",id,false)):NULL;
}
static json_t *fin_refund_get(WF_Context *ctx,json_t *body) {
    const char *id=fin_id(ctx,body,"id");if(!id)return NULL;
    json_t *u=fin_actor(ctx,body,false);if(!u)return NULL;
    json_t *r=fin_row(ctx,"refund",id,false),*o=r?fin_row(ctx,"payment",fin_s(r,"order_id"),false):NULL;
    if(!r||!o){json_decref(u);json_decref(r);json_decref(o);return NULL;}
    bool allowed=fin_admin(u)||!strcmp(fin_s(u,"user_id"),fin_s(o,"donor_id"));
    json_decref(u);json_decref(o);if(!allowed){json_decref(r);return wf_fail(ctx,403,"Refund is private");}return r;
}
static json_t *fin_refund_create(WF_Context *ctx,json_t *body) {
    const char *id=fin_id(ctx,body,"id"),*key=fin_id(ctx,body,"idempotency_key"),*amount=fin_amount(ctx,body),*reason=wf_text(ctx,body,"reason",3000);
    if(!id||!key||!amount||!reason)return NULL;
    json_t *u=fin_actor(ctx,body,false),*initial=u?fin_row(ctx,"payment",id,false):NULL;
    if(!u||!initial){json_decref(u);json_decref(initial);return NULL;}
    char pid[37];snprintf(pid,sizeof(pid),"%s",fin_s(initial,"project_id"));json_decref(initial);initial=NULL;
    json_t *project=wf_project_row(ctx,pid,true),*order=project?fin_row(ctx,"payment",id,true):NULL;
    if(!project||!order){json_decref(u);json_decref(project);json_decref(order);return NULL;}
    const char *uid=fin_s(u,"user_id"),*mode=fin_s(order,"mode");
    if(!fin_admin(u)&&strcmp(uid,fin_s(order,"donor_id"))){json_decref(u);json_decref(project);json_decref(order);return wf_fail(ctx,403,"Only the donor or administrator may request a refund");}
    const char *lookup[]={uid,key};
    json_t *existing=fin_find(ctx,"SELECT * FROM refund_request WHERE requester_id=? AND idempotency_key=? FOR UPDATE",2,lookup);
    if(!existing){json_decref(u);json_decref(project);json_decref(order);return NULL;}
    if(!json_is_null(existing)){
        bool same=!strcmp(fin_s(existing,"order_id"),id)&&fin_equal_amount(fin_s(existing,"amount"),amount)&&!strcmp(fin_s(existing,"reason"),reason);
        json_decref(u);json_decref(project);json_decref(order);
        if(!same){json_decref(existing);return wf_fail(ctx,409,"Idempotency key was used with different refund details");}return existing;
    }json_decref(existing);
    int64_t cents=fin_cents(amount);
    json_t *account=fin_account(ctx,pid,mode,false);
    bool permitted=account&&!json_is_null(account)&&!strcmp(fin_s(order,"status"),"paid")&&
        cents<=fin_cents(fin_s(order,"amount"))-fin_cents(fin_s(order,"refunded_amount"))-fin_cents(fin_s(order,"refund_reserved"))&&
        cents<=fin_cents(fin_s(account,"balance"))-fin_cents(fin_s(account,"reserved"));
    if(!permitted){json_decref(u);json_decref(project);json_decref(order);json_decref(account);if(ctx->status<400)wf_fail(ctx,409,"Refund exceeds the unrefunded order amount or available project balance");return NULL;}
    char rid[37];wf_new_id(rid);const char *p[]={rid,id,pid,uid,amount,reason,mode,key};
    bool ok=wf_exec(ctx,"INSERT INTO refund_request(id,order_id,project_id,requester_id,amount,reason,mode,idempotency_key,status,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,'pending',UTC_TIMESTAMP(),UTC_TIMESTAMP())",8,p);
    const char *reserve[]={amount,pid},*order_reserve[]={amount,id};
    if(ok)ok=wf_exec(ctx,"UPDATE project_fund_account SET reserved=reserved+?,updated_at=UTC_TIMESTAMP() WHERE project_id=?",2,reserve);
    if(ok)ok=wf_exec(ctx,"UPDATE payment_order SET refund_reserved=refund_reserved+?,updated_at=UTC_TIMESTAMP() WHERE id=?",2,order_reserve);
    if(ok&&fin_admin(u))ok=wf_admin_audit(ctx,uid,"request_refund","payment",id,reason);
    json_decref(u);json_decref(project);json_decref(order);json_decref(account);return ok?fin_row(ctx,"refund",rid,false):NULL;
}
static json_t *fin_disbursement_get(WF_Context *ctx,json_t *body) {
    const char *id=fin_id(ctx,body,"id");if(!id)return NULL;
    json_t *u=fin_actor(ctx,body,false);if(!u)return NULL;
    json_t *r=fin_row(ctx,"disbursement",id,false);
    if(r&&!fin_admin(u)&&strcmp(fin_s(r,"requester_id"),fin_s(u,"user_id"))){json_decref(r);r=wf_fail(ctx,403,"Disbursement is private");}
    json_decref(u);return fin_decode_evidence(ctx,r);
}
static bool fin_evidence_valid(WF_Context *ctx,json_t *refs,const char *uid,const char *pid) {
    return wf_files_validate_refs(ctx,refs,uid,"project",pid);
}
static json_t *fin_disbursement_create(WF_Context *ctx,json_t *body) {
    const char *pid=fin_id(ctx,body,"project_id"),*key=fin_id(ctx,body,"idempotency_key"),*amount=fin_amount(ctx,body);
    const char *purpose=wf_text(ctx,body,"purpose",3000),*destination=wf_text(ctx,body,"payout_reference",600),*mode=fin_mode(ctx,body);
    json_t *evidence=json_object_get(body,"evidence");
    if(!pid||!key||!amount||!purpose||!destination||!mode)return NULL;
    json_t *u=fin_actor(ctx,body,false),*project=u?wf_project_row(ctx,pid,true):NULL;
    if(!u||!project){json_decref(u);json_decref(project);return NULL;}
    const char *uid=fin_s(u,"user_id");
    if(strcmp(fin_s(project,"user_id"),uid)){json_decref(u);json_decref(project);return wf_fail(ctx,403,"Only the project owner may request disbursement");}
    if(!fin_evidence_valid(ctx,evidence,uid,pid)){json_decref(u);json_decref(project);return NULL;}
    char *encoded=wf_dump(ctx,evidence);if(!encoded){json_decref(u);json_decref(project);return NULL;}
    const char *lookup[]={uid,key};json_t *existing=fin_find(ctx,"SELECT * FROM fund_disbursement WHERE requester_id=? AND idempotency_key=? FOR UPDATE",2,lookup);
    if(!existing){free(encoded);json_decref(u);json_decref(project);return NULL;}
    if(!json_is_null(existing)){
        json_t *prior=wf_json_array(ctx,fin_s(existing,"evidence"));
        bool same=prior&&json_equal(prior,evidence)&&!strcmp(fin_s(existing,"project_id"),pid)&&fin_equal_amount(fin_s(existing,"amount"),amount)&&
            !strcmp(fin_s(existing,"purpose"),purpose)&&!strcmp(fin_s(existing,"payout_reference"),destination)&&!strcmp(fin_s(existing,"mode"),mode);
        json_decref(prior);free(encoded);json_decref(u);json_decref(project);
        if(!same){json_decref(existing);return wf_fail(ctx,409,"Idempotency key was used with different disbursement details");}return fin_decode_evidence(ctx,existing);
    }json_decref(existing);
    json_t *account=fin_account(ctx,pid,mode,false);
    bool permitted=wf_project_status_is(project,0)&&account&&!json_is_null(account)&&
        fin_cents(amount)<=fin_cents(fin_s(account,"balance"))-fin_cents(fin_s(account,"reserved"));
    if(!permitted){free(encoded);json_decref(u);json_decref(project);json_decref(account);if(ctx->status<400)wf_fail(ctx,409,"Project is not open or has insufficient available funds");return NULL;}
    char id[37];wf_new_id(id);const char *p[]={id,pid,uid,amount,purpose,encoded,destination,mode,key};
    bool ok=wf_exec(ctx,"INSERT INTO fund_disbursement(id,project_id,requester_id,amount,purpose,evidence,payout_reference,mode,idempotency_key,status,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,'requested',UTC_TIMESTAMP(),UTC_TIMESTAMP())",9,p);
    const char *reserve[]={amount,pid};
    if(ok)ok=wf_exec(ctx,"UPDATE project_fund_account SET reserved=reserved+?,updated_at=UTC_TIMESTAMP() WHERE project_id=?",2,reserve);
    free(encoded);json_decref(u);json_decref(project);json_decref(account);return ok?fin_decode_evidence(ctx,fin_row(ctx,"disbursement",id,false)):NULL;
}
static json_t *fin_disbursements_list(WF_Context *ctx,json_t *body) {
    bool admin=json_is_true(json_object_get(body,"admin"));json_t *u=fin_actor(ctx,body,admin);if(!u)return NULL;
    char limit[8],offset[16];if(!wa_page(ctx,body,limit,offset)){json_decref(u);return NULL;}
    const char *pid=json_string_value(json_object_get(body,"project_id"));
    if(!admin&&!pid){json_decref(u);return wf_fail(ctx,400,"Project identifier required");}
    if(pid&&!wf_uuid(pid)){json_decref(u);return wf_fail(ctx,400,"Invalid project identifier");}
    if(pid&&!fin_admin(u)){
        json_t *project=wf_project_row(ctx,pid,false);if(!project){json_decref(u);return NULL;}
        bool own=!strcmp(fin_s(project,"user_id"),fin_s(u,"user_id"));json_decref(project);
        if(!own){json_decref(u);return wf_fail(ctx,403,"Only the owner may read disbursements");}
    }
    const char *p[]={pid,limit,offset};json_t *rows=pid?
        wf_query(ctx,"SELECT * FROM fund_disbursement WHERE project_id=? ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?",3,p):
        wf_query(ctx,"SELECT * FROM fund_disbursement ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?",2,p+1);
    json_decref(u);if(!rows)return NULL;
    size_t i;json_t *row;json_array_foreach(rows,i,row){
        json_t *refs=wf_json_array(ctx,fin_s(row,"evidence"));if(!refs){json_decref(rows);return NULL;}
        json_object_set_new(row,"evidence",refs);json_object_del(row,"idempotency_key");
    }return rows;
}
static json_t *fin_disbursement_review(WF_Context *ctx,json_t *body) {
    const char *id=fin_id(ctx,body,"id"),*decision=wf_text(ctx,body,"decision",10),*note=wf_text(ctx,body,"note",3000);
    if(!id||!decision||!note)return NULL;
    if(strcmp(decision,"approve")&&strcmp(decision,"reject"))return wf_fail(ctx,400,"Invalid review decision");
    json_t *u=fin_actor(ctx,body,true),*initial=u?fin_row(ctx,"disbursement",id,false):NULL;
    if(!u||!initial){json_decref(u);json_decref(initial);return NULL;}
    char pid[37];snprintf(pid,sizeof(pid),"%s",fin_s(initial,"project_id"));json_decref(initial);initial=NULL;
    json_t *project=wf_project_row(ctx,pid,true),*r=project?fin_row(ctx,"disbursement",id,true):NULL;
    if(!project||!r){json_decref(u);json_decref(project);json_decref(r);return NULL;}
    const char *status=fin_s(r,"status");bool approve=!strcmp(decision,"approve");
    if((approve&&!strcmp(status,"approved"))||(!approve&&!strcmp(status,"rejected"))){
        json_decref(u);json_decref(project);return fin_decode_evidence(ctx,r);
    }
    if(strcmp(status,"requested")){json_decref(u);json_decref(project);json_decref(r);return wf_fail(ctx,409,"This disbursement has already been processed");}
    if(approve&&!wf_project_status_is(project,0)){json_decref(u);json_decref(project);json_decref(r);return wf_fail(ctx,409,"Only an open project can receive new disbursement approval");}
    const char *p[]={approve?"approved":"rejected",fin_s(u,"user_id"),note,id};
    bool ok=wf_exec(ctx,"UPDATE fund_disbursement SET status=?,reviewer_id=?,review_note=?,updated_at=UTC_TIMESTAMP() WHERE id=?",4,p);
    if(ok&&!approve){const char *a[]={fin_s(r,"amount"),pid};ok=wf_exec(ctx,"UPDATE project_fund_account SET reserved=reserved-?,updated_at=UTC_TIMESTAMP() WHERE project_id=?",2,a);}
    if(ok)ok=wf_admin_audit(ctx,fin_s(u,"user_id"),approve?"approve_disbursement":"reject_disbursement","disbursement",id,note);
    if(ok)ok=wf_notify(ctx,fin_s(r,"requester_id"),"disbursement_review","资金使用申请已处理",note,"/my-project");
    json_decref(u);json_decref(project);json_decref(r);return ok?fin_decode_evidence(ctx,fin_row(ctx,"disbursement",id,false)):NULL;
}
static json_t *fin_complete(WF_Context *ctx,json_t *body) {
    const char *event=wf_text(ctx,body,"event_id",128),*hash=wf_text(ctx,body,"payload_hash",64);
    const char *kind=wf_text(ctx,body,"type",16),*id=fin_id(ctx,body,"reference_id");
    const char *status=wf_text(ctx,body,"status",16),*amount=fin_amount(ctx,body),*mode=fin_mode(ctx,body);
    const char *currency=wf_text(ctx,body,"currency",3);
    if(!event||!hash||!kind||!id||!status||!amount||!mode||!currency)return NULL;
    if(strlen(hash)!=64||strcmp(currency,"CNY")||
       (strcmp(kind,"payment")&&strcmp(kind,"refund")&&strcmp(kind,"disbursement"))||
       (strcmp(status,"succeeded")&&strcmp(status,"failed")))return wf_fail(ctx,400,"Invalid payment event");
    for(size_t n=0;n<64;n++)if(!isxdigit((unsigned char)hash[n]))return wf_fail(ctx,400,"Invalid event fingerprint");
    json_t *initial=fin_row(ctx,kind,id,false);if(!initial)return NULL;
    char pid[37];snprintf(pid,sizeof(pid),"%s",fin_s(initial,"project_id"));json_decref(initial);initial=NULL;
    json_t *project=wf_project_row(ctx,pid,true),*account=project?fin_account(ctx,pid,mode,false):NULL;
    json_t *row=account?fin_row(ctx,kind,id,true):NULL,*order=NULL,*actor=NULL,*prior=NULL;
    bool ok=false,duplicate=false;
    if(!project||!account||json_is_null(account)||!row){if(ctx->status<400)wf_fail(ctx,409,"Financial account is unavailable");goto done;}
    if(strcmp(fin_s(row,"mode"),mode)||!fin_equal_amount(fin_s(row,"amount"),amount)){wf_fail(ctx,409,"Event amount or mode does not match the financial request");goto done;}
    if(!strcmp(kind,"refund")){
        order=fin_row(ctx,"payment",fin_s(row,"order_id"),true);if(!order)goto done;
    }
    if(!strcmp(mode,"development")){
        actor=fin_actor(ctx,body,false);if(!actor)goto done;
        bool permitted=fin_admin(actor);
        if(!strcmp(kind,"payment"))permitted|=!strcmp(fin_s(actor,"user_id"),fin_s(row,"donor_id"));
        if(!strcmp(kind,"refund"))permitted|=!strcmp(fin_s(actor,"user_id"),fin_s(order,"donor_id"));
        if(!permitted){wf_fail(ctx,403,"Simulation is restricted to the owner or administrator");goto done;}
    }
    const char *ep[]={event};
    prior=fin_find(ctx,"SELECT payload_hash FROM payment_event WHERE event_id=? FOR UPDATE",1,ep);
    if(!prior)goto done;
    if(!json_is_null(prior)){
        if(strcmp(fin_s(prior,"payload_hash"),hash)){wf_fail(ctx,409,"Event identifier was reused with different content");goto done;}
        ok=true;duplicate=true;goto done;
    }
    bool success=!strcmp(status,"succeeded");
    const char *old=fin_s(row,"status");
    const char *success_state=!strcmp(kind,"payment")?"paid":"succeeded";
    bool terminal=!strcmp(old,success_state)||!strcmp(old,"failed");
    if(terminal){
        if((success&&strcmp(old,success_state))||(!success&&strcmp(old,"failed"))){wf_fail(ctx,409,"A terminal financial result cannot be overwritten");goto done;}
        duplicate=true;
    }else{
        bool pending=!strcmp(kind,"payment")?!strcmp(old,"pending"):
            !strcmp(kind,"refund")?(!strcmp(old,"pending")||!strcmp(old,"processing")):
            (!strcmp(old,"approved")||!strcmp(old,"processing"));
        if(!pending){wf_fail(ctx,409,"Financial request is not ready for completion");goto done;}
        const char *update[]={success?success_state:"failed",id};
        if(!strcmp(kind,"payment")){
            ok=wf_exec(ctx,"UPDATE payment_order SET status=?,paid_at=IF(?='paid',UTC_TIMESTAMP(),NULL),updated_at=UTC_TIMESTAMP() WHERE id=?",
                3,(const char *[]){update[0],update[0],id});
            if(ok&&success){
                const char *delta[]={amount,pid};
                ok=wf_exec(ctx,"UPDATE project_fund_account SET balance=balance+?,updated_at=UTC_TIMESTAMP() WHERE project_id=?",2,delta);
                if(ok)ok=wf_exec(ctx,"UPDATE project SET current_donations_received=current_donations_received+? WHERE project_id=?",2,delta);
                if(ok)ok=wf_exec(ctx,"UPDATE user SET total_donation_amount=COALESCE(total_donation_amount,0)+? WHERE user_id=?",2,(const char *[]){amount,fin_s(row,"donor_id")});
            }
        }else if(!strcmp(kind,"refund")){
            ok=wf_exec(ctx,"UPDATE refund_request SET status=?,updated_at=UTC_TIMESTAMP(),completed_at=UTC_TIMESTAMP() WHERE id=?",2,update);
            if(ok)ok=wf_exec(ctx,"UPDATE project_fund_account SET reserved=reserved-?,balance=balance-?,updated_at=UTC_TIMESTAMP() WHERE project_id=?",
                3,(const char *[]){amount,success?amount:"0.00",pid});
            if(ok)ok=wf_exec(ctx,"UPDATE payment_order SET refund_reserved=refund_reserved-?,refunded_amount=refunded_amount+?,updated_at=UTC_TIMESTAMP() WHERE id=?",
                3,(const char *[]){amount,success?amount:"0.00",fin_s(row,"order_id")});
            if(ok&&success)ok=wf_exec(ctx,"UPDATE user SET total_donation_amount=COALESCE(total_donation_amount,0)-? WHERE user_id=?",
                2,(const char *[]){amount,fin_s(order,"donor_id")});
        }else{
            ok=wf_exec(ctx,"UPDATE fund_disbursement SET status=?,updated_at=UTC_TIMESTAMP(),completed_at=UTC_TIMESTAMP() WHERE id=?",2,update);
            if(ok)ok=wf_exec(ctx,"UPDATE project_fund_account SET reserved=reserved-?,balance=balance-?,updated_at=UTC_TIMESTAMP() WHERE project_id=?",
                3,(const char *[]){amount,success?amount:"0.00",pid});
            if(ok&&success)ok=wf_exec(ctx,"UPDATE project SET current_used_donations=current_used_donations+? WHERE project_id=?",2,(const char *[]){amount,pid});
            if(ok&&success){
                json_t *refs=wf_json_array(ctx,fin_s(row,"evidence"));
                ok=refs&&wf_files_bind(ctx,refs,fin_s(row,"requester_id"),"project",pid,json_integer_value(json_object_get(project,"submission_no")));
                json_decref(refs);
            }
        }
        if(!ok)goto done;
        if(success){
            char signed_amount[32];snprintf(signed_amount,sizeof(signed_amount),"%s%s",strcmp(kind,"payment")?"-":"",amount);
            ok=wf_exec(ctx,"INSERT INTO fund_ledger(project_id,source_type,source_id,amount,created_at) VALUES(?,?,?,?,UTC_TIMESTAMP())",
                4,(const char *[]){pid,kind,id,signed_amount});
            if(!ok)goto done;
        }
        const char *recipient=!strcmp(kind,"payment")?fin_s(row,"donor_id"):!strcmp(kind,"refund")?fin_s(order,"donor_id"):fin_s(row,"requester_id");
        ok=wf_notify(ctx,recipient,"financial_result",success?"资金操作已完成":"资金操作未完成",
            !strcmp(mode,"development")?"开发模式模拟结果，未发生真实扣款或转账。":success?"支付服务已确认处理成功。":"支付服务返回失败，请查看记录后重试。",
            !strcmp(kind,"disbursement")?"/my-project":"/donations");
        if(!ok)goto done;
    }
    ok=wf_exec(ctx,"INSERT INTO payment_event(event_id,payload_hash,source_type,source_id,status,amount,mode,created_at) VALUES(?,?,?,?,?,?,?,UTC_TIMESTAMP())",
        7,(const char *[]){event,hash,kind,id,status,amount,mode});
done:
    json_decref(initial);json_decref(project);json_decref(account);json_decref(row);
    json_decref(order);json_decref(actor);json_decref(prior);
    return ok?json_pack("{s:b,s:b}","accepted",1,"duplicate",duplicate):NULL;
}
static json_t *fin_funds(WF_Context *ctx,json_t *body) {
    const char *pid=fin_id(ctx,body,"project_id");if(!pid)return NULL;
    bool authenticated=json_object_get(body,"actor_id")!=NULL;
    json_t *u=authenticated?fin_actor(ctx,body,false):NULL;
    if(authenticated&&!u)return NULL;
    json_t *project=wf_project_row(ctx,pid,false);
    if(!project){json_decref(u);return NULL;}
    bool priv=u&&(fin_admin(u)||!strcmp(fin_s(u,"user_id"),fin_s(project,"user_id")));
    if(!priv&&!wf_project_status_is(project,0)&&!wf_project_status_is(project,13)){
        json_decref(u);json_decref(project);return wf_fail(ctx,403,"This project's funds are not public");
    }
    const char *p[]={pid};
    json_t *a=fin_find(ctx,"SELECT mode,balance,reserved FROM project_fund_account WHERE project_id=?",1,p);
    json_t *refunds=a?wf_one(ctx,"SELECT COALESCE(SUM(amount),0.00) AS refunded FROM refund_request WHERE project_id=? AND status='succeeded'",1,p):NULL;
    json_t *usage=refunds?wf_query(ctx,"SELECT id,amount,purpose,evidence,completed_at FROM fund_disbursement WHERE project_id=? AND status='succeeded' ORDER BY completed_at DESC,id DESC LIMIT 100",1,p):NULL;
    if(!a||!refunds||!usage){json_decref(u);json_decref(project);json_decref(a);json_decref(refunds);json_decref(usage);return NULL;}
    size_t i;json_t *entry;json_array_foreach(usage,i,entry){
        json_t *refs=wf_json_array(ctx,fin_s(entry,"evidence"));if(!refs){json_decref(u);json_decref(project);json_decref(a);json_decref(refunds);json_decref(usage);return NULL;}
        json_object_set_new(entry,"evidence",refs);
    }
    json_t *result=json_pack("{s:s,s:s,s:s,s:s,s:s,s:s}","project_id",pid,
        "balance",json_is_null(a)?"0.00":fin_s(a,"balance"),
        "received",fin_s(project,"current_donations_received"),"used",fin_s(project,"current_used_donations"),
        "refunded",fin_s(refunds,"refunded"),"mode",json_is_null(a)?"uninitialized":fin_s(a,"mode"));
    if(priv)json_object_set_new(result,"reserved",json_string(json_is_null(a)?"0.00":fin_s(a,"reserved")));
    json_object_set_new(result,"usage",usage);
    json_decref(u);json_decref(project);json_decref(a);json_decref(refunds);return result;
}
static json_t *fin_reconciliation(WF_Context *ctx,json_t *body){
    json_t *u=fin_actor(ctx,body,true);if(!u)return NULL;json_decref(u);
    char limit[8],offset[16];if(!wa_page(ctx,body,limit,offset))return NULL;
    const char *p[]={limit,offset};
    json_t *rows=wf_query(ctx,
        "SELECT p.project_id,COALESCE(a.balance,0.00) AS balance,COALESCE(a.reserved,0.00) AS reserved,a.mode,"
        "COALESCE((SELECT SUM(l.amount) FROM fund_ledger l WHERE l.project_id=p.project_id),0.00) AS ledger_balance,"
        "p.current_donations_received AS received,p.current_used_donations AS used,"
        "COALESCE((SELECT SUM(r.amount) FROM refund_request r WHERE r.project_id=p.project_id AND r.status='succeeded'),0.00) AS refunded "
        "FROM (SELECT project_id,current_donations_received,current_used_donations FROM project "
        "ORDER BY project_create_time DESC,project_id DESC LIMIT ? OFFSET ?) p "
        "LEFT JOIN project_fund_account a ON a.project_id=p.project_id",2,p);
    if(!rows)return NULL;
    size_t i;json_t *row;json_array_foreach(rows,i,row){
        int64_t cache=fin_cents(fin_s(row,"received"))-fin_cents(fin_s(row,"used"))-fin_cents(fin_s(row,"refunded"));
        char value[40];snprintf(value,sizeof(value),"%s%lld.%02lld",cache<0?"-":"",(long long)(llabs(cache)/100),(long long)(llabs(cache)%100));
        json_object_set_new(row,"cache_balance",json_string(value));
        json_object_set_new(row,"matches",json_boolean(cache>=0&&cache==fin_cents(fin_s(row,"balance"))&&cache==fin_cents(fin_s(row,"ledger_balance"))));
    }return rows;
}
#endif
