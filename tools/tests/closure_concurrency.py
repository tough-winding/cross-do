#!/usr/bin/env python3
"""Check production closure SQL against a stale InnoDB REPEATABLE READ snapshot.

Source the existing isolated stack.env, then run this script. It creates and
removes only its own random database and does not restart services. SQL is
extracted from wo_closure_settled in DBOP/workflow_operations.h and executed as
prepared statements, so removing its current reads fails this regression.

This is a SQL-level regression, not an HTTP/C-handler integration test. Minimal
InnoDB fixture tables isolate snapshot behavior from unrelated schema rules.
On MariaDB, innodb_snapshot_isolation is disabled only for the test reader
session: ON may abort with error 1020 instead of returning a current read. No
global variable or application connection is changed.
"""

import argparse
import json
import os
from pathlib import Path
import re
import select
import subprocess
import sys

from schema_integration import Database, Failure, require, sql_text


ROOT = Path(__file__).resolve().parents[2]
PROJECT = "11111111-1111-4111-8111-111111111111"
EXPECTED_TABLES = ("project_fund_account", "payment_order", "refund_request",
                   "fund_disbursement", "fund_ledger")


def production_queries():
    source = (ROOT / "DBOP/workflow_operations.h").read_text(encoding="utf-8")
    start = source.find("static bool wo_closure_settled(")
    end = source.find("\nstatic json_t *wf_ops_closure_review(", start)
    require(start >= 0 and end > start, "closure helper moved; update the SQL extractor")
    function = source[start:end]
    literal = r'"(?:[^"\\]|\\.)*"'
    pattern = r'wf_(?:query|one)\(ctx,\s*((?:' + literal + r'\s*)+),\s*(\d+),\s*(\w+)\)'
    queries = []
    for match in re.finditer(pattern, function):
        sql = "".join(json.loads(token) for token in re.findall(literal, match.group(1)))
        count = int(match.group(2))
        params = match.group(3)
        require(sql.count("?") == count, "production placeholder count changed")
        require(params in ("params", "refund_params"), "production parameter layout changed")
        queries.append((sql, params))
    require(len(queries) == len(EXPECTED_TABLES), "production helper no longer has five readable finance checks")
    for (sql, _), table in zip(queries, EXPECTED_TABLES):
        require(re.search(r"\bFROM\s+" + table + r"\b", sql, re.I) is not None,
                "production finance check order changed")
    return queries


class Reader:
    def __init__(self, db, database):
        self.timeout = db.options.timeout
        self.process = subprocess.Popen(db.command + ["--unbuffered", "--database=" + database],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                        text=True, env=db.environment, bufsize=1)

    def query(self, sql):
        try:
            self.process.stdin.write(sql + ";\n")
            self.process.stdin.flush()
        except (OSError, BrokenPipeError) as error:
            raise Failure("test database reader disconnected") from error
        ready, _, _ = select.select([self.process.stdout], [], [], self.timeout)
        require(bool(ready), "test database reader timed out")
        result = self.process.stdout.readline()
        require(bool(result), "test reader SQL failed; client diagnostics suppressed")
        return result.strip()

    def prepared(self, sql, values):
        require(sql.count("?") == len(values), "test parameter count differs from production")
        assignments = ["SET @crossdo_closure_sql=" + sql_text(sql)]
        assignments += ["SET @crossdo_closure_{}={}".format(i, sql_text(value))
                        for i, value in enumerate(values)]
        variables = ",".join("@crossdo_closure_{}".format(i) for i in range(len(values)))
        return self.query(";".join(assignments + [
            "PREPARE crossdo_closure_stmt FROM @crossdo_closure_sql",
            "EXECUTE crossdo_closure_stmt USING " + variables,
            "DEALLOCATE PREPARE crossdo_closure_stmt"]))

    def close(self):
        if self.process.poll() is None:
            self.process.stdin.close()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        self.process.stdout.close()


def check(options):
    queries = production_queries()
    db = Database(options)
    reader = None
    try:
        database = db.create("rr")
        target = sql_text(PROJECT)
        db.execute(
            "CREATE TABLE project(project_id CHAR(36) PRIMARY KEY,current_donations_received DECIMAL(14,2),"
            "current_used_donations DECIMAL(14,2)) ENGINE=InnoDB;"
            "INSERT INTO project VALUES({target},0,0);"
            "CREATE TABLE project_fund_account(project_id CHAR(36) PRIMARY KEY,balance DECIMAL(14,2),"
            "reserved DECIMAL(14,2)) ENGINE=InnoDB;INSERT INTO project_fund_account VALUES({target},0,0);"
            "CREATE TABLE payment_order(id INT PRIMARY KEY,project_id CHAR(36),status VARCHAR(16)) ENGINE=InnoDB;"
            "CREATE TABLE refund_request(id INT PRIMARY KEY,project_id CHAR(36),status VARCHAR(16),amount DECIMAL(10,2)) ENGINE=InnoDB;"
            "CREATE TABLE fund_disbursement(id INT PRIMARY KEY,project_id CHAR(36),status VARCHAR(16)) ENGINE=InnoDB;"
            "CREATE TABLE fund_ledger(id INT PRIMARY KEY,project_id CHAR(36),amount DECIMAL(14,2)) ENGINE=InnoDB;"
            .format(target=target), database)
        global_snapshot = db.execute("SHOW GLOBAL VARIABLES LIKE 'innodb_snapshot_isolation';")
        reader = Reader(db, database)
        configure = "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;"
        if global_snapshot:
            configure += "SET SESSION innodb_snapshot_isolation=OFF;"
            print("INFO MariaDB snapshot isolation disabled for the test reader session only", flush=True)
        # Model the router's early nonlocking account read by opening a snapshot
        # before a second connection commits new funds and pending operations.
        require(reader.query(configure + "START TRANSACTION; SELECT balance FROM project_fund_account "
                             "WHERE project_id=" + target) == "0.00", "initial balance must be zero")
        db.execute(
            "START TRANSACTION;SELECT project_id FROM project WHERE project_id={target} FOR UPDATE;"
            "UPDATE project SET current_donations_received=10 WHERE project_id={target};"
            "UPDATE project_fund_account SET balance=10,reserved=2 WHERE project_id={target};"
            "INSERT INTO payment_order VALUES(1,{target},'pending');"
            "INSERT INTO refund_request VALUES(1,{target},'pending',2),(2,{target},'succeeded',3);"
            "INSERT INTO fund_disbursement VALUES(1,{target},'approved');"
            "INSERT INTO fund_ledger VALUES(1,{target},10);COMMIT;".format(target=target), database)
        # The production helper receives the already locked project row.
        project = reader.query("SELECT current_donations_received,current_used_donations FROM project "
                               "WHERE project_id=" + target + " FOR UPDATE").split("\t")
        require(project == ["10.00", "0.00"], "project current read missed committed receipt")
        require(reader.query("SELECT balance FROM project_fund_account WHERE project_id=" + target) == "0.00",
                "fixture failed to retain the stale nonlocking snapshot")
        for (sql, params), table, expected in zip(queries, EXPECTED_TABLES, ("0", "1", "0", "1", "0")):
            values = project + [PROJECT] if params == "refund_params" else [PROJECT]
            require(reader.prepared(sql, values) == expected,
                    "production closure check missed concurrent changes in " + table)
        require(reader.query("SELECT COALESCE(SUM(amount),0)=0 FROM fund_ledger WHERE project_id=" + target) == "1",
                "fixture's ordinary aggregate should still read the old zero ledger")
        require(reader.query("ROLLBACK;SELECT 'rolled_back'") == "rolled_back", "reader rollback failed")
        require(db.execute("SHOW GLOBAL VARIABLES LIKE 'innodb_snapshot_isolation';") == global_snapshot,
                "global snapshot isolation setting changed")
        print("PASS production closure SQL sees newly committed balances, pending records and ledger despite an older snapshot")
    finally:
        if reader:
            reader.close()
        db.cleanup()


def options_from_environment():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mysql-binary", default=os.environ.get("CROSSDO_TEST_MYSQL_BINARY", "mysql"))
    parser.add_argument("--mysql-user", default=os.environ.get("CROSSDO_TEST_MYSQL_USER", "root"))
    parser.add_argument("--mysql-socket", default=os.environ.get("CROSSDO_TEST_MYSQL_SOCKET"))
    parser.add_argument("--mysql-host", default=os.environ.get("CROSSDO_TEST_MYSQL_HOST", "127.0.0.1"))
    parser.add_argument("--mysql-port", type=int, default=int(os.environ.get("CROSSDO_TEST_MYSQL_PORT", "3306")))
    parser.add_argument("--database-prefix", default="crossdo_closure_check",
                        help="prefix for a new random database, never an existing database name")
    parser.add_argument("--timeout", type=int, default=30)
    options = parser.parse_args()
    require(bool(re.fullmatch(r"[a-z0-9_]{1,32}", options.database_prefix)), "invalid temporary database prefix")
    require(1 <= options.timeout <= 300, "timeout must be 1..300 seconds")
    options.mysql_password = os.environ.get("CROSSDO_TEST_MYSQL_PASSWORD")
    return options


def main():
    try:
        check(options_from_environment())
        return 0
    except (Failure, OSError, subprocess.SubprocessError) as error:
        # SQL and connection credentials are never included in client output.
        print("FAIL " + (str(error) if isinstance(error, Failure) else "test database client unavailable"), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
