#!/usr/bin/env python3
"""Grant administrator access to one existing, phone-verified UUID offline.

The default is a read-only preview; --apply performs the grant and appends an
audit record in one transaction. No account or password is created. Each applied
run advances session_version, including a repeat grant, revoking older sessions.
Requires Python 3 and the mysql (or mariadb) command-line client.
"""

import argparse
import getpass
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import uuid


class BootstrapError(Exception):
    pass


def sql_text(value):
    """Use hex text literals so neither SQL mode nor shell escaping matters."""
    return "CONVERT(0x{} USING utf8mb4)".format(value.encode("utf-8").hex())


def options(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--database", required=True, help="explicit target database; never inferred from the application")
    parser.add_argument("--user-id", required=True, help="existing account UUID whose phone_verified must be 1")
    parser.add_argument("--note", required=True, help="operator identity / authorization ticket and reason, 1..1300 characters")
    parser.add_argument("--apply", action="store_true", help="commit the grant; without this flag only preview the account")
    parser.add_argument("--mysql-binary", default="mysql", help="mysql or mariadb executable path")
    parser.add_argument("--defaults-file", type=Path, help="read only this MySQL client option file (for credentials/TLS)")
    parser.add_argument("--mysql-user", help="database login; may instead be set in --defaults-file")
    parser.add_argument("--mysql-host", help="TCP host; omit to use the option file or client local default")
    parser.add_argument("--mysql-port", type=int, help="TCP port")
    parser.add_argument("--mysql-socket", help="local Unix socket, mutually exclusive with host/port")
    parser.add_argument("--password-env", default="CROSSDO_ADMIN_MYSQL_PASSWORD", help="environment variable holding DB password")
    parser.add_argument("--ask-password", action="store_true", help="prompt privately for DB password instead of reading the environment")
    parser.add_argument("--timeout", type=int, default=30, help="client timeout in seconds (default: 30)")
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[A-Za-z0-9_]{1,64}", args.database):
        parser.error("--database must contain 1..64 letters, digits or underscores")
    if not re.fullmatch(r"[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}", args.user_id):
        parser.error("--user-id must be a UUID")
    args.user_id = str(uuid.UUID(args.user_id))
    if not args.note.strip() or len(args.note) > 1300 or "\0" in args.note:
        parser.error("--note must contain 1..1300 characters and no NUL")
    if not args.mysql_user and not args.defaults_file:
        parser.error("supply --mysql-user or --defaults-file; no database login is assumed")
    if args.mysql_socket and (args.mysql_host or args.mysql_port):
        parser.error("--mysql-socket cannot be combined with --mysql-host/--mysql-port")
    if args.mysql_port is not None and not 1 <= args.mysql_port <= 65535:
        parser.error("--mysql-port must be 1..65535")
    if not 1 <= args.timeout <= 300:
        parser.error("--timeout must be 1..300")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", args.password_env):
        parser.error("--password-env must be an environment-variable name")
    if args.defaults_file:
        args.defaults_file = args.defaults_file.resolve()
        if not args.defaults_file.is_file():
            parser.error("--defaults-file must refer to an existing file")
    return args


class Client:
    def __init__(self, args):
        self.timeout = args.timeout
        self.command = [args.mysql_binary,
                        "--defaults-file=" + str(args.defaults_file) if args.defaults_file else "--no-defaults",
                        "--batch", "--skip-force", "--raw", "--skip-column-names", "--default-character-set=utf8mb4",
                        "--database=" + args.database]
        if args.mysql_user:
            self.command.append("--user=" + args.mysql_user)
        if args.mysql_socket:
            self.command += ["--protocol=socket", "--socket=" + args.mysql_socket]
        elif args.mysql_host or args.mysql_port:
            self.command.append("--protocol=tcp")
            if args.mysql_host:
                self.command.append("--host=" + args.mysql_host)
            if args.mysql_port:
                self.command.append("--port=" + str(args.mysql_port))
        self.environment = os.environ.copy()
        self.environment.pop("MYSQL_PWD", None)
        password = getpass.getpass("Database password: ") if args.ask_password else os.environ.get(args.password_env)
        if password is not None:
            self.environment["MYSQL_PWD"] = password

    def execute(self, sql, *, audit_id=None):
        try:
            result = subprocess.run(self.command, input=sql, text=True, capture_output=True,
                                    env=self.environment, timeout=self.timeout, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            detail = "Database client could not complete."
            if audit_id:
                detail += " Outcome may be committed; inspect admin_audit id {} before retrying.".format(audit_id)
            raise BootstrapError(detail) from error
        if result.returncode != 0:
            # MySQL may echo statements containing credentials or personal data;
            # never relay its stdout/stderr. Batch mode stops on the first error,
            # and disconnect rolls back an uncommitted update/audit transaction.
            detail = "Database operation failed; check connectivity, migrations 003/004 and privileges."
            if audit_id:
                detail += " Verify audit id {} before retrying if connectivity was interrupted.".format(audit_id)
            raise BootstrapError(detail)
        lines = [line for line in result.stdout.splitlines() if line.strip()]
        try:
            return json.loads(lines[-1]) if lines else None
        except (ValueError, TypeError) as error:
            raise BootstrapError("Database returned an unexpected result; verify audit before retrying.") from error


def account_query(identifier):
    return ("SELECT JSON_OBJECT('user_id',user_id,'role',user_permission,'user_status',user_status,"
            "'phone_verified',phone_verified,'account_state',account_state,"
            "'session_version',CAST(session_version AS CHAR)) FROM `user` WHERE user_id={};\n").format(sql_text(identifier))


def bootstrap(args):
    client = Client(args)
    account = client.execute(account_query(args.user_id))
    if not account:
        raise BootstrapError("Account UUID does not exist; no account was created.")
    if account.get("phone_verified") != 1:
        raise BootstrapError("Account phone is not verified; complete the normal SMS verification flow first.")
    if not args.apply:
        return {"applied": False, "database": args.database, "account": account,
                "planned": {"role": 1, "user_status": 0, "real_name_authentication": 1,
                            "account_state": "active", "session_version": str(int(account["session_version"]) + 1)}}
    audit_id = str(uuid.uuid4())
    target = sql_text(args.user_id)
    sql = ("SET time_zone='+00:00';\nSTART TRANSACTION;\n"
           "UPDATE `user` SET user_permission=1,user_status=0,real_name_authentication=1,"
           "account_state='active',session_version=session_version+1 "
           "WHERE user_id={target} AND phone_verified=1;\n"
           "SET @crossdo_bootstrap_changed=ROW_COUNT();\n"
           "INSERT INTO admin_audit(id,actor_id,action,target_type,target_id,note,created_at) "
           "SELECT {audit},{target},'bootstrap_admin','user',{target},"
           "CONCAT('Offline bootstrap; database_operator=',CURRENT_USER(),'; ',{note}),UTC_TIMESTAMP() "
           "WHERE @crossdo_bootstrap_changed=1;\n"
           "COMMIT;\n"
           "SELECT JSON_OBJECT('applied',@crossdo_bootstrap_changed,'audit_id',{audit},'user_id',{target});\n"
           ).format(target=target, audit=sql_text(audit_id), note=sql_text(args.note.strip()))
    result = client.execute(sql, audit_id=audit_id)
    # MariaDB represents integer-valued user variables as JSON strings, while
    # MySQL may preserve their numeric type.
    if not result or str(result.get("applied")) != "1":
        raise BootstrapError("Account disappeared or lost phone verification; no grant or audit was written.")
    result["applied"] = True
    result["database"] = args.database
    result["sessions_revoked"] = True
    return result


def main(argv=None):
    try:
        result = bootstrap(options(argv))
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))
        return 0
    except BootstrapError as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
