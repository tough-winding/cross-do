#!/usr/bin/env python3
"""Isolated MariaDB, Redis, DBOP and IAM stack for the workflow integration suite.

Usage: start_workflow_stack.sh [start|stop|status]
The Java server is launched separately after sourcing the generated private env file.
No host MySQL/Redis configurations, existing database or production data are used.
"""

import hashlib
import json
import os
from pathlib import Path
import secrets
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import time
import urllib.request


REPO = Path(__file__).resolve().parents[2]
STATE = Path("/tmp/crossdo-review-stack")
TOOLS = Path(os.environ.get("CROSSDO_TOOLS", "/workspace/.crossdo-tools"))
PORTS = {"mysql": 13316, "redis": 16379, "dbop": 11900, "iam": 11089}
DATABASE = "crossdo_review_test"


def executable(name, relative=None):
    candidate = TOOLS / "sysroot" / (relative or ("usr/bin/" + name))
    if candidate.is_file():
        return str(candidate)
    found = shutil.which(name)
    if not found:
        raise RuntimeError("Missing executable: " + name)
    return found


def ticks(pid):
    try:
        # /proc's parenthesized process name may itself contain whitespace.
        fields = Path("/proc/{}/stat".format(pid)).read_text().rsplit(")", 1)[1].split()
        return None if fields[0] == "Z" else fields[19]
    except (FileNotFoundError, IndexError, ProcessLookupError):
        return None


def owned_process(record):
    return ticks(record["pid"]) == record["ticks"]


def load_processes():
    file = STATE / "processes.json"
    return json.loads(file.read_text()) if file.exists() else {}


def save_processes(processes):
    (STATE / "processes.json").write_text(json.dumps(processes))


def launch(name, command, processes, cwd=None):
    with (STATE / (name + "-console.log")).open("ab") as log:
        environment = os.environ.copy()
        environment["CROSSDO_BIND_ADDRESS"] = "127.0.0.1"
        child = subprocess.Popen(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT,
                                 stdin=subprocess.DEVNULL, start_new_session=True, env=environment)
    processes[name] = {"pid": child.pid, "ticks": ticks(child.pid)}
    save_processes(processes)


def wait_port(name, processes):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if not owned_process(processes[name]):
            raise RuntimeError(name + " exited; inspect its private logs in " + str(STATE))
        try:
            with socket.create_connection(("127.0.0.1", PORTS[name]), timeout=0.2):
                return
        except OSError:
            time.sleep(0.15)
    raise RuntimeError(name + " did not become ready; inspect logs in " + str(STATE))


def run(command, *, data=None, log_name="setup.log"):
    with (STATE / log_name).open("ab") as log:
        completed = subprocess.run(command, input=data, stdout=log, stderr=subprocess.STDOUT)
    if completed.returncode:
        raise RuntimeError("Setup command failed; inspect " + str(STATE / log_name))


def stop():
    processes = load_processes()
    for name in ("iam", "dbop", "redis", "mysql"):
        record = processes.get(name)
        if record and owned_process(record):
            os.kill(record["pid"], signal.SIGTERM)
            deadline = time.monotonic() + 15
            while owned_process(record) and time.monotonic() < deadline:
                time.sleep(0.1)
            if owned_process(record):
                os.kill(record["pid"], signal.SIGKILL)
    if STATE.exists():
        save_processes({})
    print("Stopped only the isolated workflow dependency processes.")


def status():
    processes = load_processes()
    for name, port in PORTS.items():
        running = name in processes and owned_process(processes[name])
        print("{}: {} (127.0.0.1:{})".format(name, "running" if running else "stopped", port))
    return all(name in processes and owned_process(processes[name]) for name in PORTS)


def write_environment(credentials):
    # Keep the integration boundary token independent of Java's IAM token refresh.
    if "test" not in credentials:
        credentials["test"] = secrets.token_hex(24)
        (STATE / "secrets.json").write_text(json.dumps(credentials))
    mysql = [executable("mariadb"), "--no-defaults", "--protocol=socket", "--socket=" + str(STATE / "mysql.sock"),
             "--user=root", DATABASE]
    sql = ("INSERT INTO service(service_name,service_passwd) VALUES ('CROSSDO_TEST','{}') "
           "ON DUPLICATE KEY UPDATE service_passwd=VALUES(service_passwd);\n").format(credentials["test"])
    run(mysql, data=sql.encode())
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())
    signature = hashlib.sha256(("CROSSDO_TEST+" + credentials["test"] + "+" + timestamp).encode()).hexdigest()
    token_request = urllib.request.Request("http://127.0.0.1:11089/get_token", method="POST",
        data=json.dumps({"service_username": "CROSSDO_TEST", "encrypt_str": signature, "date": timestamp}).encode(),
        headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(token_request, timeout=5) as response:
        token = json.loads(response.read())["Token"]
    variables = {
        "SERVER_PORT": "18080", "SPRING_REDIS_HOST": "127.0.0.1", "SPRING_REDIS_PORT": "16379",
        "SPRING_REDIS_PASSWORD": "", "CROSSDO_DBOP_URL": "http://127.0.0.1:11900",
        "CROSSDO_IAM_URL": "http://127.0.0.1:11089", "CROSSDO_SERVICE_NAME": "CROSSDO_WEB",
        "CROSSDO_SERVICE_PASSWORD": credentials["web"],
        "CROSSDO_SMS_MODE": "development", "CROSSDO_PAYMENT_MODE": "development",
        "CROSSDO_UPLOAD_DIR": str(STATE / "uploads"),
        "CROSSDO_TEST_JAVA_URL": "http://127.0.0.1:18080", "CROSSDO_TEST_DBOP_URL": "http://127.0.0.1:11900",
        "CROSSDO_TEST_MYSQL_DATABASE": DATABASE, "CROSSDO_TEST_MYSQL_SOCKET": str(STATE / "mysql.sock"),
        "CROSSDO_TEST_MYSQL_BINARY": executable("mariadb"),
        "CROSSDO_TEST_SERVICE_NAME": "CROSSDO_TEST", "CROSSDO_TEST_DBOP_TOKEN": token,
    }
    (STATE / "stack.env").write_text("".join("export {}={}\n".format(key, shlex.quote(value))
                                               for key, value in variables.items()))


def start():
    os.umask(0o077)
    STATE.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(STATE, 0o700)
    processes = load_processes()
    if any(owned_process(record) for record in processes.values()):
        if status():
            write_environment(json.loads((STATE / "secrets.json").read_text()))
            print("Private Java/test environment: " + str(STATE / "stack.env"))
            return
        raise RuntimeError("A partial isolated stack is running; stop it before starting again")
    for name, port in PORTS.items():
        with socket.socket() as probe:
            try:
                probe.bind(("127.0.0.1", port))
            except OSError:
                raise RuntimeError("Port {} is already occupied; no existing process was changed".format(port))
    processes = {}
    binary_dir = STATE / "bin"
    binary_dir.mkdir(exist_ok=True)
    run(["gcc", "-D_POSIX_C_SOURCE=200809L", "-std=c99", "-O2", "-o", str(binary_dir / "dbop"),
         str(REPO / "DBOP/database_operation.c"), "-levent", "-ljansson", "-lzlog", "-lyaml",
         "-luuid", "-lhiredis", os.environ.get("MYSQL_LIB", "-lmariadb"), "-lpthread"], log_name="compile.log")
    run(["gcc", "-std=c99", "-O2", "-o", str(binary_dir / "iam"),
         str(REPO / "IAM/identity_access_management.c"), "-lyaml", "-lhiredis", "-lcurl", "-levent",
         "-levent_pthreads", "-lssl", "-lcrypto", "-lpthread", "-lzlog", "-luuid", "-ljansson"],
        log_name="compile.log")

    secrets_path = STATE / "secrets.json"
    if secrets_path.exists():
        credentials = json.loads(secrets_path.read_text())
    else:
        credentials = {key: secrets.token_hex(24) for key in ("database", "web", "iam")}
        secrets_path.write_text(json.dumps(credentials))
    datadir = STATE / "mysql"
    sysroot_base = TOOLS / "sysroot/usr"
    install = [executable("mariadb-install-db"), "--no-defaults", "--datadir=" + str(datadir),
               "--auth-root-authentication-method=normal", "--skip-test-db"]
    if sysroot_base.exists():
        install.append("--basedir=" + str(sysroot_base))
    if not (datadir / "mysql").exists():
        run(install)
    server = [executable("mariadbd", "usr/sbin/mariadbd"), "--no-defaults", "--datadir=" + str(datadir),
              "--socket=" + str(STATE / "mysql.sock"), "--pid-file=" + str(STATE / "mysql.pid"),
              "--log-error=" + str(STATE / "mysql.log"), "--bind-address=127.0.0.1", "--port=13316",
              "--skip-name-resolve", "--innodb-buffer-pool-size=64M", "--max-connections=30"]
    if sysroot_base.exists():
        server.extend(["--basedir=" + str(sysroot_base),
                       "--plugin-dir=" + str(sysroot_base / "lib/mysql/plugin"),
                       "--lc-messages-dir=" + str(sysroot_base / "share/mysql")])
    launch("mysql", server, processes)
    wait_port("mysql", processes)
    mysql = [executable("mariadb"), "--no-defaults", "--protocol=socket", "--socket=" + str(STATE / "mysql.sock"),
             "--user=root"]
    db_password = credentials["database"]
    admin_sql = ("CREATE DATABASE IF NOT EXISTS `{}` CHARACTER SET utf8mb4;\n"
                 "CREATE USER IF NOT EXISTS 'crossdo_review'@'127.0.0.1' IDENTIFIED BY '{}';\n"
                 "GRANT ALL PRIVILEGES ON `{}`.* TO 'crossdo_review'@'127.0.0.1';\n").format(
                     DATABASE, db_password, DATABASE)
    run(mysql, data=admin_sql.encode())
    probe = subprocess.run(mysql + ["--batch", "--skip-column-names", DATABASE],
        input=b"SELECT COUNT(*) FROM information_schema.tables WHERE table_schema=DATABASE() AND table_name='user';",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    if probe.stdout.strip() == b"0":
        run(mysql + [DATABASE], data=(REPO / "设计/schema.sql").read_bytes())
    service_sql = ("INSERT INTO service(service_name,service_passwd) VALUES ('CROSSDO_WEB','{}'),"
                   "('IAM_SERVICE','{}') ON DUPLICATE KEY UPDATE service_passwd=VALUES(service_passwd);\n").format(
                       credentials["web"], credentials["iam"])
    run(mysql + [DATABASE], data=service_sql.encode())
    # Exercise repeatable migration on the freshly initialized schema too.
    for migration_path in sorted((REPO / "设计/migrations").glob("*.sql")):
        migration = migration_path.read_bytes()
        run(mysql + [DATABASE], data=migration)
        run(mysql + [DATABASE], data=migration)

    launch("redis", [executable("redis-server"), "--bind", "127.0.0.1", "--port", "16379",
                     "--protected-mode", "yes", "--save", "", "--appendonly", "no", "--dir", str(STATE)], processes)
    wait_port("redis", processes)
    for name in ("dbop", "iam"):
        configuration = STATE / name / "config"
        configuration.mkdir(parents=True, exist_ok=True)
        (configuration / "zlog.conf").write_text(
            '[global]\nstrict init = true\n[formats]\nstd = "%d %V %m%n"\n[rules]\n*.INFO "' +
            str(STATE / (name + ".log")) + '"; std\n')
    dbop_config = STATE / "dbop/config/db_config.yaml"
    dbop_config.write_text(
        'server_port: "11900"\nlog_file: "' + str(STATE / "dbop.log") + '"\nlog_config: "' +
        str(STATE / "dbop/config/zlog.conf") + '"\ndatabase_ip: "127.0.0.1"\ndatabase_port: "13316"\n'
        'database_privilege_user: "crossdo_review"\ndatabase_privilege_password: "' + db_password + '"\n'
        'user_database_name: "' + DATABASE + '"\ndatabase_pool_size: "3"\nredis_server: "127.0.0.1"\n'
        'redis_port: "16379"\nredis_password: ""\nredis_pool_size: "3"\n')
    (STATE / "iam/config/iam_config.yaml").write_text(
        'redis_server: "127.0.0.1"\nredis_port: "16379"\nredis_password: ""\n'
        'dbop_service_url: "127.0.0.1"\ndbop_service_port: "11900"\n'
        'server_ip: "127.0.0.1"\nserver_port: "11089"\nlog_config: "' +
        str(STATE / "iam/config/zlog.conf") + '"\n')
    launch("dbop", [str(binary_dir / "dbop"), str(dbop_config)], processes, STATE / "dbop")
    wait_port("dbop", processes)
    launch("iam", [str(binary_dir / "iam")], processes, STATE / "iam")
    wait_port("iam", processes)

    write_environment(credentials)
    print("Isolated MySQL, Redis, DBOP and IAM are ready.")
    print("Java/test environment: source " + str(STATE / "stack.env"))
    print("Java is started separately; its configured port is 18080.")


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "start"
    if mode not in ("start", "stop", "status"):
        raise RuntimeError("Usage: start_workflow_stack.sh [start|stop|status]")
    if mode == "stop":
        stop()
    elif mode == "status":
        return 0 if status() else 1
    else:
        start()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, KeyError, ValueError) as error:
        print("Workflow stack: " + str(error), file=sys.stderr)
        sys.exit(1)
