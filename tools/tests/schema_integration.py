#!/usr/bin/env python3
"""Exercise schema integrity and all migrations on disposable databases.

The MySQL account needs CREATE/DROP DATABASE and routine privileges. This suite
never selects the application's configured database: every case creates a new
random database, and only databases successfully created by this process are
removed. Connection settings follow the workflow test environment.

Example:
  source /tmp/crossdo-review-stack/stack.env
  python3 tools/tests/schema_integration.py

No third-party Python modules are required. SQL errors are kept out of output so
connection secrets and records cannot be printed by a failed command.
"""

import argparse
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import sys
import uuid


ROOT = Path(__file__).resolve().parents[2]
LEGACY = Path(__file__).parent / "fixtures/legacy_schema.sql"
SCHEMA = ROOT / "设计/schema.sql"
MIGRATIONS = sorted(p for p in (ROOT / "设计/migrations").glob("*.sql") if "preflight" not in p.stem)


class Failure(AssertionError):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def sql_text(value):
    """A UTF-8 hex literal avoids SQL mode and string-escaping ambiguity."""
    if value is None:
        return "NULL"
    encoded = str(value).encode("utf-8").hex()
    return "CONVERT(0x{} USING utf8mb4)".format(encoded) if encoded else "''"


def sql_json(value):
    return sql_text(json.dumps(value, ensure_ascii=False, separators=(",", ":")))


class Database:
    def __init__(self, options):
        self.options = options
        self.created = []
        self.command = [options.mysql_binary, "--no-defaults", "--batch", "--raw",
                        "--skip-column-names", "--default-character-set=utf8mb4",
                        "--user=" + options.mysql_user]
        if options.mysql_socket:
            self.command += ["--protocol=socket", "--socket=" + options.mysql_socket]
        else:
            self.command += ["--protocol=tcp", "--host=" + options.mysql_host,
                             "--port=" + str(options.mysql_port)]
        self.environment = os.environ.copy()
        if options.mysql_password is not None:
            self.environment["MYSQL_PWD"] = options.mysql_password

    def execute(self, statement, database=None, *, reject=False):
        command = self.command + (["--database=" + database] if database else [])
        result = subprocess.run(command, input=statement, text=True, capture_output=True,
                                env=self.environment, timeout=self.options.timeout)
        if reject:
            require(result.returncode != 0, "database accepted an invalid write or migration")
        else:
            require(result.returncode == 0, "SQL operation failed (exit {})".format(result.returncode))
        return result.stdout.strip()

    def create(self, label):
        name = "{}_{}_{}".format(self.options.database_prefix, label, secrets.token_hex(6))
        require(bool(re.fullmatch(r"[a-z0-9_]{1,64}", name)), "invalid temporary database name")
        # No IF NOT EXISTS: a collision must fail, never adopt a pre-existing DB.
        self.execute("CREATE DATABASE `{}` CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;".format(name))
        self.created.append(name)
        return name

    def import_file(self, path, database, *, reject=False):
        return self.execute(path.read_text(encoding="utf-8"), database, reject=reject)

    def rows(self, query, database):
        result = self.execute(query, database)
        return [line.split("\t") for line in result.splitlines()] if result else []

    def cleanup(self):
        for name in reversed(self.created):
            # This allowlist contains only CREATE DATABASE calls that succeeded.
            self.execute("DROP DATABASE `{}`;".format(name))
        self.created.clear()


class Suite:
    def __init__(self, options):
        self.db = Database(options)
        self.groups = 0
        self.rejected = 0

    def passed(self, message):
        self.groups += 1
        print("PASS {:02d} {}".format(self.groups, message), flush=True)

    def reject(self, statement, database):
        self.db.execute(statement, database, reject=True)
        self.rejected += 1

    def add_user(self, database, *, role=3, status=0, verified=1):
        identifier = str(uuid.uuid4())
        self.db.execute(
            "INSERT INTO `user` (user_id,user_permission,user_name,user_password,"
            "real_name_authentication,user_status,existing_projects,project_id,phone_number,"
            "registration_date,total_donation_amount) VALUES ({},{},'schema_test',{},"
            "{},{},0,NULL,{},'2026-01-01 00:00:00',0.00);".format(
                sql_text(identifier), role, sql_text("$2b$12$" + "a" * 53), verified, status,
                sql_text("19" + "{:09d}".format(secrets.randbelow(10**9)))), database)
        return identifier

    def project_insert(self, owner, identifier=None):
        identifier = identifier or str(uuid.uuid4())
        return identifier, (
            "INSERT INTO project (project_id,user_id,user_name,user_age,real_name,project_status,"
            "verification_record,current_volunteer,previous_volunteers,question_is_volunteer_assigned,"
            "project_create_time,donation_pool_limit,current_donations_received,current_used_donations,"
            "last_update_time,pathography) VALUES ({},{},'schema_test',24,'测试姓名',1,"
            "'[]',NULL,'[]',0,'2026-01-01 00:00:00',100.25,5.02,1.01,'2026-01-01 00:00:00','[]');"
        ).format(sql_text(identifier), sql_text(owner))

    def add_project(self, database, owner):
        identifier, insert = self.project_insert(owner)
        self.db.execute(insert, database)
        return identifier

    @staticmethod
    def application_insert(owner, volunteer=None, materials=None, history=None, previous=None):
        return (
            "INSERT INTO patient_application (user_id,materials,current_volunteer,previous_volunteers,"
            "verification_record,submitted_at,last_change_time) VALUES ({},{},{},{},{},"
            "'2026-01-01 00:00:00','2026-01-01 00:00:00');"
        ).format(sql_text(owner), sql_json(materials or ["https://example.invalid/material.pdf"]),
                 sql_text(volunteer), sql_json(previous or []), sql_json(history or []))

    def constraint_checks(self):
        database = self.db.create("fresh")
        self.db.import_file(SCHEMA, database)
        owner = self.add_user(database)
        other = self.add_user(database)
        volunteer = self.add_user(database, role=5)
        project = self.add_project(database, owner)
        other_project = self.add_project(database, other)
        project_sql = sql_text(project)
        owner_sql = sql_text(owner)
        missing = str(uuid.uuid4())
        self.db.execute("UPDATE `user` SET existing_projects=1,project_id={} WHERE user_id={};"
                        .format(project_sql, owner_sql), database)
        self.db.execute(self.application_insert(owner), database)

        _, orphan = self.project_insert(missing)
        self.reject(orphan, database)
        self.reject(self.application_insert(missing), database)
        self.reject("UPDATE patient_application SET current_volunteer={} WHERE user_id={};"
                    .format(sql_text(missing), owner_sql), database)
        self.reject("UPDATE project SET current_volunteer={},question_is_volunteer_assigned=1 WHERE project_id={};"
                    .format(sql_text(missing), project_sql), database)
        self.reject("UPDATE `user` SET project_id={} WHERE user_id={};"
                    .format(sql_text(other_project), owner_sql), database)
        self.reject("DELETE FROM `user` WHERE user_id={};".format(owner_sql), database)
        self.passed("foreign keys reject orphan applications/projects and mismatched project ownership")

        for assignment in ("user_permission=999", "user_status=14", "user_status=NULL",
                           "real_name_authentication=2", "existing_projects=0",
                           "total_donation_amount=-0.01"):
            self.reject("UPDATE `user` SET {} WHERE user_id={};".format(assignment, owner_sql), database)
        for assignment in ("project_status=14", "project_status=NULL", "donation_pool_limit=-0.01",
                           "current_donations_received=-0.01", "current_used_donations=5.03",
                           "project_status=0,donation_pool_limit=0.00"):
            self.reject("UPDATE project SET {} WHERE project_id={};".format(assignment, project_sql), database)
        self.passed("database rejects invalid states, booleans, negative amounts and overspending")

        _, duplicate = self.project_insert(owner)
        self.reject(duplicate, database)
        self.db.execute("UPDATE project SET project_status=13 WHERE project_id={};".format(project_sql), database)
        replacement = self.add_project(database, owner)
        self.db.execute("UPDATE `user` SET project_id={} WHERE user_id={};"
                        .format(sql_text(replacement), owner_sql), database)
        require(self.db.execute("SELECT COUNT(*) FROM project WHERE user_id={};".format(owner_sql), database) == "2",
                "closed project should allow a new project without deleting its history")
        self.passed("one active project per patient is enforced while closed projects remain archived")

        event = {"actor_id": volunteer, "decision": "claim", "note": "schema test", "time": "2026-01-01 00:00:00"}
        insert = ("INSERT INTO application_review_event "
                  "(user_id,sequence_no,actor_id,decision,recorded_at,record) VALUES ({},1,{},'claim',"
                  "'2026-01-01 00:00:00',{});").format(owner_sql, sql_text(volunteer), sql_json(event))
        self.db.execute(insert, database)
        self.reject(insert, database)
        self.reject("INSERT INTO application_review_event(user_id,sequence_no,record) VALUES ({},1,'{{}}');"
                    .format(sql_text(missing)), database)
        self.reject("INSERT INTO project_review_event(project_id,sequence_no,actor_id,record) VALUES ({},1,{},'{{}}');"
                    .format(project_sql, sql_text(missing)), database)
        self.reject("INSERT INTO project_review_event(project_id,sequence_no,record) VALUES ({},1,'broken-json');"
                    .format(project_sql), database)
        entry = {"content": "第一版病情", "attachments": [], "time": "2026-01-01 00:00:00"}
        path_insert = "INSERT INTO project_pathography_entry(project_id,submission_no,sequence_no,record) VALUES ({},0,1,{});".format(project_sql, sql_json(entry))
        self.db.execute(path_insert, database)
        self.reject(path_insert, database)
        self.db.execute("INSERT INTO project_pathography_entry(project_id,submission_no,sequence_no,record) VALUES ({},1,1,{});"
                        .format(project_sql, sql_json({**entry, "content": "第二版病情"})), database)
        require(self.db.execute("SELECT COUNT(DISTINCT submission_no) FROM project_pathography_entry WHERE project_id={};"
                                .format(project_sql), database) == "2", "submission versions were not retained")
        self.passed("event ordering and versioned pathography keep unique sequence numbers and valid JSON")
        return database

    def legacy_seed(self, database):
        self.db.import_file(LEGACY, database)
        owner = self.add_user(database)
        volunteer = self.add_user(database, role=5)
        other = self.add_user(database, role=2, verified=0)
        project = self.add_project(database, owner)
        materials = ["https://example.invalid/material.pdf"]
        unknown_actor = str(uuid.uuid4())
        history = [
            {"actor_id": owner, "decision": "submit", "note": "提交材料", "time": "2026-01-01 01:02:03", "materials": materials},
            {"actor_id": volunteer, "decision": "claim", "note": "接单", "time": "2026-01-01 02:03:04"},
            {"actor_id": unknown_actor, "decision": "old_review", "note": "历史记录保留", "time": "unknown"},
            {"角色": "患者", "内容": "旧版自定义记录"},
        ]
        pathography = [
            {"content": "原始病情一", "attachments": [], "time": "2026-01-01 01:02:03"},
            {"content": "原始病情二", "attachments": ["https://example.invalid/scan.png"], "time": "2026-01-02 01:02:03"},
        ]
        previous = [volunteer, unknown_actor]
        self.db.execute(self.application_insert(owner, materials=materials, history=history, previous=previous), database)
        self.db.execute(
            "UPDATE project SET verification_record={},pathography={},previous_volunteers={} WHERE project_id={};"
            "UPDATE `user` SET existing_projects=1,project_id={} WHERE user_id={};"
            "UPDATE `user` SET total_donation_amount=NULL,avatar=NULL,id_card=NULL WHERE user_id={};".format(
                sql_json(history), sql_json(pathography), sql_json(previous), sql_text(project),
                sql_text(project), sql_text(owner), sql_text(other)), database)
        return {"owner": owner, "volunteer": volunteer, "project": project,
                "history": history, "pathography": pathography, "previous": previous}

    def snapshot(self, database):
        """Check source snapshots byte-for-byte, including SQL NULL values."""
        return self.db.execute(
            "SELECT user_id,user_permission,user_status,real_name_authentication,existing_projects,project_id,"
            "total_donation_amount FROM `user` ORDER BY user_id;"
            "SELECT project_id,user_id,project_status,donation_pool_limit,current_donations_received,"
            "current_used_donations,HEX(verification_record),HEX(previous_volunteers),HEX(pathography) "
            "FROM project ORDER BY project_id;"
            "SELECT user_id,HEX(materials),current_volunteer,HEX(verification_record),HEX(previous_volunteers) "
            "FROM patient_application ORDER BY user_id;", database)

    def structure(self, database):
        # Column order is not part of the API; an ALTER may append a new column.
        query = (
            "SELECT TABLE_NAME,COLUMN_NAME,COLUMN_TYPE,IS_NULLABLE,COALESCE(COLUMN_DEFAULT,'<SQL_NULL>'),EXTRA,"
            "COALESCE(GENERATION_EXPRESSION,''),COALESCE(CHARACTER_SET_NAME,''),COALESCE(COLLATION_NAME,'') "
            "FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() "
            "ORDER BY TABLE_NAME,COLUMN_NAME;"
            "SELECT TABLE_NAME,INDEX_NAME,NON_UNIQUE,SEQ_IN_INDEX,COLUMN_NAME,COALESCE(SUB_PART,0) "
            "FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() ORDER BY TABLE_NAME,INDEX_NAME,SEQ_IN_INDEX;"
            "SELECT TABLE_NAME,CONSTRAINT_NAME,CONSTRAINT_TYPE FROM information_schema.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA=DATABASE() ORDER BY TABLE_NAME,CONSTRAINT_NAME;"
            "SELECT TABLE_NAME,CONSTRAINT_NAME,COLUMN_NAME,ORDINAL_POSITION,"
            "COALESCE(REFERENCED_TABLE_NAME,''),COALESCE(REFERENCED_COLUMN_NAME,'') "
            "FROM information_schema.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA=DATABASE() "
            "ORDER BY TABLE_NAME,CONSTRAINT_NAME,ORDINAL_POSITION;"
            "SELECT TABLE_NAME,CONSTRAINT_NAME,UPDATE_RULE,DELETE_RULE "
            "FROM information_schema.REFERENTIAL_CONSTRAINTS WHERE CONSTRAINT_SCHEMA=DATABASE() "
            "ORDER BY TABLE_NAME,CONSTRAINT_NAME;"
            "SELECT CONSTRAINT_NAME,CHECK_CLAUSE FROM information_schema.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA=DATABASE() ORDER BY CONSTRAINT_NAME;"
        )
        return self.db.execute(query, database)

    def release_constraint_checks(self, database, owner, reviewer, project):
        """Seed each additive module and reject corruption through direct SQL writes."""
        now = "2026-01-04 00:00:00"
        missing = str(uuid.uuid4())

        def insert(table, values, *, reject=False):
            statement = "INSERT INTO `{}` ({}) VALUES ({});".format(
                table, ",".join("`{}`".format(key) for key in values),
                ",".join(sql_text(value) for value in values.values()))
            if reject:
                self.reject(statement, database)
            else:
                self.db.execute(statement, database)

        def invalid_updates(table, key, identifier, assignments):
            for assignment in assignments:
                self.reject("UPDATE `{}` SET {} WHERE `{}`={};".format(
                    table, assignment, key, sql_text(identifier)), database)

        invalid_updates("user", "user_id", owner, ["account_state='unknown'", "session_version=-1",
                                                  "phone_verified=2", "phone_verified=NULL"])
        notification = {"id": str(uuid.uuid4()), "user_id": owner, "type": "schema_test",
                        "title": "test", "body": "test", "link": "/account", "created_at": now}
        insert("notification", notification)
        insert("notification", {**notification, "id": str(uuid.uuid4()), "user_id": missing}, reject=True)
        insert("admin_audit", {"id": str(uuid.uuid4()), "actor_id": reviewer, "action": "test",
                               "target_type": "project", "target_id": project, "note": "test", "created_at": now})
        case = {"id": str(uuid.uuid4()), "reporter_id": owner, "target_user_id": reviewer,
                "kind": "report", "reason": "schema test", "evidence": "[]", "created_at": now, "updated_at": now}
        insert("case_report", case)
        invalid_updates("case_report", "id", case["id"], ["target_user_id=NULL", "project_id=" + sql_text(project),
                        "kind='unknown'", "status='unknown'", "evidence='{}'", "parent_id=" + sql_text(missing)])
        insert("case_event", {"case_id": case["id"], "actor_id": reviewer, "action": "comment",
                              "note": "schema test", "created_at": now})
        insert("project_closure", {"project_id": project, "requester_id": owner, "reason": "schema test",
                                   "remaining_plan": "refund", "requested_at": now})
        invalid_updates("project_closure", "project_id", project, ["remaining_plan='unknown'", "status='unknown'"])
        self.passed("operations and security enforce account, phone, case target/evidence and closure constraints")

        file = {"id": str(uuid.uuid4()), "owner_id": owner, "purpose": "application",
                "storage_key": str(uuid.uuid4()), "original_name": "proof.pdf", "mime_type": "application/pdf",
                "byte_size": 10, "sha256": "a" * 64, "created_at": now}
        insert("uploaded_file", file)
        invalid_updates("uploaded_file", "id", file["id"], ["purpose='unknown'", "purpose='project'",
                        "project_id=" + sql_text(project), "mime_type='text/html'", "byte_size=0", "byte_size=10485761",
                        "owner_id=" + sql_text(missing)])
        insert("uploaded_file", {**file, "id": str(uuid.uuid4())}, reject=True)
        project_file = {**file, "id": str(uuid.uuid4()), "storage_key": str(uuid.uuid4()),
                        "purpose": "project", "project_id": project}
        insert("uploaded_file", project_file)
        insert("uploaded_file", {**file, "id": str(uuid.uuid4()), "storage_key": str(uuid.uuid4()), "purpose": "case"})
        usage = {"file_id": file["id"], "application_user_id": owner, "created_at": now}
        insert("file_usage", usage)
        insert("file_usage", usage, reject=True)
        insert("file_usage", {**usage, "project_id": project}, reject=True)
        insert("file_usage", {**usage, "submission_no": 1}, reject=True)
        insert("file_usage", {"file_id": file["id"], "created_at": now}, reject=True)
        insert("file_usage", {**usage, "file_id": missing}, reject=True)
        project_usage = {"file_id": project_file["id"], "project_id": project, "submission_no": 1, "created_at": now}
        insert("file_usage", project_usage)
        insert("file_usage", {**project_usage, "submission_no": 2})
        insert("file_usage", project_usage, reject=True)
        self.passed("files enforce MIME, size, ownership references and unique target/version usage")

        insert("project_fund_account", {"project_id": project, "mode": "development", "balance": "5.02",
                                       "reserved": "1.01", "updated_at": now})
        invalid_updates("project_fund_account", "project_id", project,
                        ["mode='unknown'", "balance=-0.01", "reserved=-0.01", "reserved=5.03"])
        order = {"id": str(uuid.uuid4()), "project_id": project, "donor_id": owner, "amount": "5.02",
                 "mode": "development", "idempotency_key": str(uuid.uuid4()), "created_at": now, "updated_at": now}
        insert("payment_order", order)
        invalid_updates("payment_order", "id", order["id"], ["amount=0", "currency='USD'", "status='unknown'",
                        "mode='unknown'", "refunded_amount=-0.01", "refund_reserved=-0.01",
                        "refunded_amount=3.00,refund_reserved=2.03", "donor_id=" + sql_text(missing)])
        insert("payment_order", {**order, "id": str(uuid.uuid4())}, reject=True)
        refund = {"id": str(uuid.uuid4()), "order_id": order["id"], "project_id": project, "requester_id": owner,
                  "amount": "1.01", "reason": "test", "mode": "development", "idempotency_key": str(uuid.uuid4()),
                  "created_at": now, "updated_at": now}
        insert("refund_request", refund)
        invalid_updates("refund_request", "id", refund["id"], ["amount=0", "status='unknown'", "mode='unknown'",
                        "order_id=" + sql_text(missing)])
        insert("refund_request", {**refund, "id": str(uuid.uuid4())}, reject=True)
        self.passed("fund accounts, payment and refund amounts, states, foreign keys and idempotency are enforced")

        disbursement = {"id": str(uuid.uuid4()), "project_id": project, "requester_id": owner, "amount": "1.01",
                        "purpose": "test", "evidence": "[]", "payout_reference": "development:test",
                        "mode": "development", "idempotency_key": str(uuid.uuid4()), "created_at": now, "updated_at": now}
        insert("fund_disbursement", disbursement)
        invalid_updates("fund_disbursement", "id", disbursement["id"], ["amount=0", "evidence='{}'", "status='unknown'",
                        "mode='unknown'", "reviewer_id=" + sql_text(missing)])
        insert("fund_disbursement", {**disbursement, "id": str(uuid.uuid4())}, reject=True)
        ledger = {"project_id": project, "source_type": "payment", "source_id": order["id"], "amount": "5.02", "created_at": now}
        insert("fund_ledger", ledger)
        insert("fund_ledger", ledger, reject=True)
        insert("fund_ledger", {**ledger, "source_id": str(uuid.uuid4()), "amount": "-0.01"}, reject=True)
        insert("fund_ledger", {**ledger, "source_type": "refund", "source_id": refund["id"]}, reject=True)
        insert("fund_ledger", {**ledger, "source_type": "refund", "source_id": refund["id"], "amount": "-1.01"})
        insert("payment_event", {"event_id": "schema-event", "payload_hash": "b" * 64, "source_type": "payment",
                                 "source_id": order["id"], "status": "succeeded", "amount": "5.02", "mode": "development",
                                 "created_at": now})
        invalid_updates("payment_event", "event_id", "schema-event", ["amount=0", "source_type='unknown'",
                        "status='unknown'", "mode='unknown'"])
        self.passed("disbursements and callback/ledger records enforce valid evidence, states and signed unique entries")

    def release_snapshot(self, database):
        tables = ("notification", "admin_audit", "case_report", "case_event", "project_closure",
                  "uploaded_file", "file_usage", "project_fund_account", "payment_order", "refund_request",
                  "fund_disbursement", "fund_ledger", "payment_event")
        return self.db.execute("".join("SELECT * FROM `{}` ORDER BY 1;".format(table) for table in tables), database)

    def migration_checks(self, fresh):
        database = self.db.create("legacy")
        expected = self.legacy_seed(database)
        before = self.snapshot(database)
        for migration in MIGRATIONS:
            self.db.import_file(migration, database)
        require(self.snapshot(database) == before, "migration changed the original source records")
        for table, key, identifier in (("application_review_event", "user_id", expected["owner"]),
                                       ("project_review_event", "project_id", expected["project"])):
            data = self.db.rows("SELECT record,legacy_import FROM {} WHERE {}={} ORDER BY sequence_no;"
                                .format(table, key, sql_text(identifier)), database)
            require([json.loads(row[0]) for row in data] == expected["history"], "migration lost or rewrote legacy event data")
            require(all(row[1] == "1" for row in data), "legacy events were not marked as imported")
            actors = self.db.rows("SELECT actor_id FROM {} WHERE {}={} ORDER BY sequence_no;"
                                  .format(table, key, sql_text(identifier)), database)
            require([row[0] for row in actors] == [expected["owner"], expected["volunteer"], "NULL", "NULL"],
                    "event actor links must use only existing accounts while retaining original JSON")
        data = self.db.rows("SELECT record FROM project_pathography_entry WHERE project_id={} AND submission_no=0 ORDER BY sequence_no;"
                            .format(sql_text(expected["project"])), database)
        require([json.loads(row[0]) for row in data] == expected["pathography"], "legacy pathography was not preserved as version zero")
        self.passed("legacy JSON, unknown history fields and nullable data survive migration unchanged")

        self.release_constraint_checks(database, expected["owner"], expected["volunteer"], expected["project"])
        release_before = self.release_snapshot(database)
        marker_before = self.db.execute("SELECT version,applied_at FROM schema_migration ORDER BY version;", database)
        # Migration 002 introduced the marker table; 001 predates tracking.
        require([row[0] for row in self.db.rows("SELECT version FROM schema_migration ORDER BY version;", database)]
                == [migration.stem for migration in MIGRATIONS if int(migration.name[:3]) >= 2],
                "not all tracked migrations were marked complete")

        runtime = {"actor_id": expected["volunteer"], "decision": "approve", "note": "new event", "time": "2026-01-03 00:00:00"}
        self.db.execute("INSERT INTO project_review_event(project_id,sequence_no,actor_id,decision,recorded_at,record) "
                        "VALUES ({},5,{},'approve','2026-01-03 00:00:00',{});".format(
                            sql_text(expected["project"]), sql_text(expected["volunteer"]), sql_json(runtime)), database)
        for migration in MIGRATIONS:
            self.db.import_file(migration, database)
        require(self.snapshot(database) == before, "repeated migration changed legacy records")
        require(self.release_snapshot(database) == release_before, "repeated migrations changed release module records")
        require(self.db.execute("SELECT version,applied_at FROM schema_migration ORDER BY version;", database) == marker_before,
                "repeated migrations rewrote completion markers")
        require(self.db.execute("SELECT COUNT(*) FROM application_review_event;", database) == "4", "repeated migration duplicated application events")
        require(self.db.execute("SELECT COUNT(*) FROM project_review_event;", database) == "5", "repeated migration duplicated or erased project events")
        require(self.db.execute("SELECT COUNT(*) FROM project_pathography_entry;", database) == "2", "repeated migration duplicated pathography")
        record = self.db.execute("SELECT record FROM project_review_event WHERE project_id={} AND sequence_no=5;"
                                 .format(sql_text(expected["project"])), database)
        require(json.loads(record) == runtime, "repeated migration overwrote a new event")
        self.passed("all migrations are repeatable without changing source data, runtime records or completion markers")
        require(self.structure(database) == self.structure(fresh), "fresh and migrated schemas differ in columns, indexes or constraints")
        self.passed("fresh installation and upgraded installation have matching columns, indexes and constraints")

    def dirty_data_checks(self):
        for label, assignment, repair in (
            ("json", "verification_record='not-json'", "verification_record='[]'"),
            ("money", "current_donations_received=-1.00", "current_donations_received=5.02"),
            ("orphan", "user_id={}".format(sql_text(str(uuid.uuid4()))), None),
        ):
            database = self.db.create(label)
            expected = self.legacy_seed(database)
            self.db.import_file(MIGRATIONS[0], database)
            project_sql = sql_text(expected["project"])
            self.db.execute("UPDATE project SET {} WHERE project_id={};".format(assignment, project_sql), database)
            before, structure = self.snapshot(database), self.structure(database)
            self.db.import_file(MIGRATIONS[1], database, reject=True)
            require(self.snapshot(database) == before, "failed preflight changed invalid source data")
            require(self.structure(database) == structure, "failed preflight changed database tables")
            if repair is None:
                repair = "user_id={}".format(sql_text(expected["owner"]))
            self.db.execute("UPDATE project SET {} WHERE project_id={};".format(repair, project_sql), database)
            self.db.import_file(MIGRATIONS[1], database)
        self.passed("invalid history, amounts and orphan relations stop migration before table changes; repaired data can retry")

    def bootstrap_guard_checks(self, fresh):
        before, structure = self.snapshot(fresh), self.structure(fresh)
        marker = self.db.execute("SELECT version,applied_at FROM schema_migration ORDER BY version;", fresh)
        self.db.import_file(SCHEMA, fresh)
        require(self.snapshot(fresh) == before and self.structure(fresh) == structure,
                "re-importing the completed bootstrap changed existing data or structure")
        require(self.db.execute("SELECT version,applied_at FROM schema_migration ORDER BY version;", fresh) == marker,
                "re-importing the completed bootstrap rewrote its migration marker")

        database = self.db.create("guard")
        self.legacy_seed(database)
        for has_marker_table in (False, True):
            if has_marker_table:
                self.db.execute("CREATE TABLE schema_migration (version VARCHAR(64) NOT NULL PRIMARY KEY, "
                                "applied_at DATETIME NOT NULL) ENGINE=InnoDB;", database)
            before, structure = self.snapshot(database), self.structure(database)
            self.db.import_file(SCHEMA, database, reject=True)
            require(self.snapshot(database) == before, "rejected bootstrap changed legacy source data")
            require(self.structure(database) == structure, "rejected bootstrap changed legacy tables")
            if has_marker_table:
                require(self.db.execute("SELECT COUNT(*) FROM schema_migration;", database) == "0",
                        "rejected bootstrap incorrectly marked an incomplete migration as complete")
            else:
                require(self.db.execute("SELECT COUNT(*) FROM information_schema.tables "
                                        "WHERE table_schema=DATABASE() AND table_name='schema_migration';", database) == "0",
                        "rejected bootstrap created a migration marker table")
        self.passed("bootstrap re-import is safe and refuses unmigrated or partially migrated databases")

    def run(self):
        fresh = self.constraint_checks()
        self.bootstrap_guard_checks(fresh)
        self.migration_checks(fresh)
        self.dirty_data_checks()
        print("Completed {} schema groups; {} invalid direct writes rejected.".format(self.groups, self.rejected))


def options_from_environment():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mysql-binary", default=os.environ.get("CROSSDO_TEST_MYSQL_BINARY", "mysql"))
    parser.add_argument("--mysql-user", default=os.environ.get("CROSSDO_TEST_MYSQL_USER", "root"))
    parser.add_argument("--mysql-socket", default=os.environ.get("CROSSDO_TEST_MYSQL_SOCKET"))
    parser.add_argument("--mysql-host", default=os.environ.get("CROSSDO_TEST_MYSQL_HOST", "127.0.0.1"))
    parser.add_argument("--mysql-port", type=int, default=int(os.environ.get("CROSSDO_TEST_MYSQL_PORT", "3306")))
    parser.add_argument("--database-prefix", default=os.environ.get("CROSSDO_SCHEMA_DATABASE_PREFIX", "crossdo_schema_check"),
                        help="prefix for new random databases, never the name of an existing database")
    parser.add_argument("--timeout", type=int, default=120)
    options = parser.parse_args()
    require(bool(re.fullmatch(r"[a-z0-9_]{1,32}", options.database_prefix)), "database prefix must be 1..32 lowercase letters, digits or underscores")
    options.mysql_password = os.environ.get("CROSSDO_TEST_MYSQL_PASSWORD")
    return options


def main():
    suite = None
    result = 0
    try:
        suite = Suite(options_from_environment())
        suite.run()
    except (AssertionError, OSError, ValueError, subprocess.SubprocessError) as error:
        print("FAIL " + (str(error) if isinstance(error, Failure) else type(error).__name__), file=sys.stderr)
        result = 1
    finally:
        if suite:
            try:
                suite.db.cleanup()
            except (AssertionError, OSError, subprocess.SubprocessError):
                print("FAIL temporary database cleanup failed", file=sys.stderr)
                result = 1
    return result


if __name__ == "__main__":
    sys.exit(main())
