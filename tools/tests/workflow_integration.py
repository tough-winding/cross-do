#!/usr/bin/env python3
r"""Exercise the Java API and C DBOP against an isolated MySQL/Redis setup.

Only Python's standard library and a MySQL-compatible command-line client are
required. All HTTP test accounts are newly registered. The only direct SQL
mutation certifies those newly created volunteer accounts; no production API
can grant that certification. Never point this script at a production database.

Example (the servers must already be running):
  CROSSDO_TEST_JAVA_URL=http://127.0.0.1:18080 \
  CROSSDO_TEST_DBOP_URL=http://127.0.0.1:11900 \
  CROSSDO_TEST_MYSQL_DATABASE=crossdo_test \
  CROSSDO_TEST_MYSQL_SOCKET=/tmp/crossdo-test/mysql.sock \
  python3 tools/tests/workflow_integration.py

The optional CROSSDO_TEST_DBOP_TOKEN / CROSSDO_TEST_SERVICE_NAME values enable
direct checks of the internal C boundary. Authentication secrets are never
printed. Test accounts are deliberately retained for diagnosis.
"""

import argparse
import concurrent.futures
import json
import os
import secrets
import subprocess
import sys
import threading
import urllib.error
import urllib.parse
import urllib.request
import uuid
from decimal import Decimal


class Failure(AssertionError):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def field(value, name, default=None):
    """Accept Python/SQL-style names and Jackson's usual camel-case spelling."""
    if not isinstance(value, dict):
        return default
    parts = name.split("_")
    camel = parts[0] + "".join(part.title() for part in parts[1:])
    return value.get(name, value.get(camel, default))


def unwrap(value):
    if isinstance(value, dict) and "data" in value:
        return value["data"]
    return value


def records(value, name):
    stored = field(value, name, [])
    if stored is None or stored == "":
        return []
    parsed = json.loads(stored) if isinstance(stored, str) else stored
    require(isinstance(parsed, list), name + " must be a JSON array")
    return parsed


class Client:
    def __init__(self, base_url, timeout):
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout

    def request(self, path, body=None, token=None, headers=None, method="POST"):
        sent_headers = {"Content-Type": "application/json"}
        if token:
            sent_headers["Authorization"] = "Bearer " + token
        if headers:
            sent_headers.update(headers)
        request = urllib.request.Request(
            self.base_url + path,
            data=json.dumps(body or {}).encode("utf-8") if method != "GET" else None,
            headers=sent_headers,
            method=method,
        )
        try:
            response = urllib.request.urlopen(request, timeout=self.timeout)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            raw = response.read()
            try:
                data = json.loads(raw.decode("utf-8")) if raw else None
            except (UnicodeDecodeError, ValueError):
                raise Failure("{} returned non-JSON HTTP {}".format(path, response.code))
            return response.code, unwrap(data)

    def ok(self, path, body=None, token=None, headers=None, method="POST"):
        status, data = self.request(path, body, token, headers, method)
        require(200 <= status < 300, "{} expected success, received HTTP {}".format(path, status))
        return data

    def reject(self, path, body=None, token=None, allowed=(400, 401, 403, 409, 422), method="POST"):
        status, data = self.request(path, body, token, method=method)
        require(status in allowed, "{} expected rejection {}, received HTTP {}".format(path, allowed, status))
        return data


class Suite:
    def __init__(self, options):
        self.options = options
        self.api = Client(options.java_url, options.timeout)
        self.internal = Client(options.dbop_url, options.timeout)
        self.accounts = []
        self.checks = 0

    def passed(self, message):
        self.checks += 1
        print("PASS {:02d} {}".format(self.checks, message), flush=True)

    @staticmethod
    def route(name, body):
        payload = dict(body)
        if name == "account/get":
            return "/api/me", payload, "GET"
        if name == "application/submit":
            return "/api/me/application", payload, "POST"
        if name == "application/list":
            return "/api/applications?" + urllib.parse.urlencode(payload), {}, "GET"
        if name.startswith("application/"):
            target = payload.pop("user_id")
            action = name.split("/")[1]
            path = "/api/applications/" + target
            return path + ("/" + action if action != "get" else ""), payload, "GET" if action == "get" else "POST"
        if name in ("project/list", "project/tasks"):
            path = "/api/projects" if name == "project/list" else "/api/tasks/projects"
            return path + "?" + urllib.parse.urlencode(payload), {}, "GET"
        if name == "project/create":
            return "/api/projects", payload, "POST"
        if name.startswith("project/"):
            target = payload.pop("project_id")
            action = name.split("/")[1]
            path = "/api/projects/" + target
            return path + ("/" + action if action != "get" else ""), payload, "GET" if action == "get" else "POST"
        raise Failure("No Java REST route for " + name)

    def action(self, name, body, account):
        path, payload, method = self.route(name, body)
        return self.api.ok(path, payload, account["token"], method=method)

    def reject(self, name, body, account, allowed=(400, 401, 403, 409, 422)):
        path, payload, method = self.route(name, body)
        return self.api.reject(path, payload, account["token"], allowed, method)

    def register(self, permission):
        suffix = secrets.token_hex(5)
        phone = "19" + "{:09d}".format(secrets.randbelow(10**9))
        password = "Test-" + secrets.token_urlsafe(18)
        body = {"phone_number": phone, "user_name": "test_" + suffix,
                "password": password, "user_permission": permission}
        challenge = self.api.ok("/api/auth/verification-codes", {"phone_number": phone, "purpose": "register"})
        require(bool(field(challenge, "development_code")), "integration requires explicitly enabled development SMS mode")
        body.update(challenge_id=field(challenge, "challenge_id"), code=field(challenge, "development_code"))
        data = self.api.ok("/api/auth/register", body)
        account_data = field(data, "user", field(data, "account", data))
        user_id = field(account_data, "user_id", field(data, "user_id"))
        require(isinstance(user_id, str) and str(uuid.UUID(user_id)) == user_id,
                "register must return a canonical user UUID")
        account = {"user_id": user_id, "phone_number": phone,
                   "password": password, "user_permission": permission}
        self.accounts.append(account)
        return account

    def login(self, account):
        data = self.api.ok("/api/auth/login", {
            "phone_number": account["phone_number"], "password": account["password"]})
        token = field(data, "token", field(data, "access_token"))
        require(isinstance(token, str) and bool(token), "login must return a bearer token")
        account["token"] = token
        return account

    def certify_test_volunteers(self, accounts):
        options = self.options
        require(bool(options.mysql_database), "set CROSSDO_TEST_MYSQL_DATABASE to an isolated test database")
        command = [options.mysql_binary, "--batch", "--skip-column-names",
                   "--database=" + options.mysql_database, "--user=" + options.mysql_user]
        if options.mysql_socket:
            command += ["--protocol=socket", "--socket=" + options.mysql_socket]
        else:
            command += ["--protocol=tcp", "--host=" + options.mysql_host,
                        "--port=" + str(options.mysql_port)]
        identifiers = [str(uuid.UUID(account["user_id"])) for account in accounts]
        sql = "UPDATE `user` SET user_status=0, real_name_authentication=1 WHERE user_permission=5 AND user_id IN ({}); SELECT ROW_COUNT();".format(
            ",".join("'" + value + "'" for value in identifiers))
        environment = os.environ.copy()
        if options.mysql_password is not None:
            environment["MYSQL_PWD"] = options.mysql_password
        result = subprocess.run(command, input=sql, text=True, capture_output=True,
                                timeout=options.timeout, env=environment)
        require(result.returncode == 0, "test database certification failed (mysql exit {})".format(result.returncode))
        require(result.stdout.strip() == str(len(accounts)), "test database did not certify exactly the new volunteers")

    @staticmethod
    def application_payload():
        return {"real_name": "测试患者", "id_card": "11010519491231002X", "user_age": 36,
                "materials": ["https://example.test/medical-report.pdf",
                              "https://example.test/identity-proof.jpg"]}

    def race_claim(self, action, body, volunteers):
        barrier = threading.Barrier(len(volunteers))

        def claim(volunteer):
            barrier.wait(timeout=self.options.timeout)
            path, payload, method = self.route(action, body)
            status, data = self.api.request(path, payload, volunteer["token"], method=method)
            return volunteer, status, data

        with concurrent.futures.ThreadPoolExecutor(max_workers=len(volunteers)) as executor:
            results = list(executor.map(claim, volunteers))
        winners = [account for account, status, _ in results if 200 <= status < 300]
        require(len(winners) == 1, "concurrent claims must produce exactly one winner")
        require(all(200 <= status < 300 or status == 409 for _, status, _ in results),
                "a competing claim must return HTTP 409")
        return winners[0]

    def assert_private(self, value):
        encoded = json.dumps(value, ensure_ascii=False)
        for secret in ("user_password", "password_hash", "id_card", "weixin_openid"):
            require(secret not in encoded, "public response exposes " + secret)
        require("11010519491231002X" not in encoded, "response exposes the complete identity number")

    def run(self):
        require(bool(self.options.mysql_database),
                "set CROSSDO_TEST_MYSQL_DATABASE to an isolated test database before running")
        donor = self.login(self.register(2))
        patient = self.login(self.register(3))
        outsider = self.login(self.register(3))
        volunteers = [self.register(5), self.register(5)]
        self.certify_test_volunteers(volunteers)
        volunteers = [self.login(volunteer) for volunteer in volunteers]
        challenge = self.api.ok("/api/auth/verification-codes", {"phone_number": donor["phone_number"], "purpose": "password_reset"})
        self.api.reject("/api/auth/register", {
            "phone_number": donor["phone_number"], "user_name": "duplicate",
            "password": donor["password"], "user_permission": 2,
            "challenge_id": field(challenge, "challenge_id"), "code": field(challenge, "development_code")}, allowed=(400, 409))
        self.api.reject("/api/auth/register", {
            "phone_number": "19" + "{:09d}".format(secrets.randbelow(10**9)),
            "user_name": "forbidden_admin", "password": "Never-register-admin-123",
            "user_permission": 1}, allowed=(400, 403, 422))
        self.passed("register and log in independent donor, patients and certified test volunteers")

        self.api.reject("/api/me", allowed=(401,), method="GET")
        self.api.reject("/api/me", token="invalid-token", allowed=(401,), method="GET")
        self.api.reject("/api/auth/login", {"phone_number": patient["phone_number"], "password": "Wrong-password-123"}, allowed=(401,))
        self.reject("project/create", {}, patient, allowed=(403, 409))
        self.reject("application/submit", self.application_payload(), donor, allowed=(403, 409))
        self.passed("unauthenticated requests, wrong password, uncertified project creation and wrong roles fail")

        self.reject("application/submit", dict(self.application_payload(), materials=["file:///private/report.pdf"]),
                    patient, allowed=(400, 422))
        # Each submission is valid on its own, but two snapshots exceed the old
        # TEXT ceiling. Repeated corrections must not exhaust a shared blob.
        large_application = dict(self.application_payload(), materials=[
            "https://example.test/material/{}/".format(i) + "x" * 1850 for i in range(20)])
        submitted = self.action("application/submit", large_application, patient)
        require(field(submitted, "user_status") == 2, "first application submission must enter state 2")
        application_id = {"user_id": patient["user_id"]}
        applications = self.action("application/list", {"limit": 100, "offset": 0}, volunteers[0])
        require(isinstance(applications, list) and any(field(item, "user_id") == patient["user_id"] for item in applications),
                "submitted patient must appear in application tasks")
        self.assert_private(applications)
        self.reject("application/get", application_id, outsider, allowed=(403,))
        self.reject("application/claim", application_id, donor, allowed=(403,))
        self.reject("application/claim", dict(application_id, actor_id=volunteers[0]["user_id"]), donor, allowed=(403,))
        winner = self.race_claim("application/claim", application_id, volunteers)
        loser = next(account for account in volunteers if account is not winner)
        self.reject("application/review", dict(application_id, decision="approve", note="越权"), loser, allowed=(403, 409))
        self.passed("application ownership and actor spoofing are blocked; one volunteer wins concurrent claims")

        self.action("application/review", dict(application_id, decision="correction", note="补充医生签字"), winner)
        first = self.action("application/get", application_id, patient)
        require(field(first, "user_status") == 5, "correction must enter application state 5")
        first_history = records(first, "verification_record")
        require(bool(first_history), "application corrections must append history")
        resubmitted = self.action("application/submit", large_application, patient)
        require(field(resubmitted, "user_status") == 6, "application re-submission must enter state 6")
        self.action("application/claim", application_id, volunteers[0])
        self.action("application/review", dict(application_id, decision="approve", note="身份及资料已核验"), volunteers[0])
        approved = self.action("application/get", application_id, patient)
        require(field(approved, "user_status") == 0, "approved application must enter state 0")
        history = records(approved, "verification_record")
        require(len(history) > len(first_history), "application re-submission must preserve earlier review history")
        require(all(entry in history for entry in first_history), "application review history was overwritten")
        require(len(json.dumps(history).encode()) > 65535, "fixture must exceed the old history TEXT limit")
        require(records(approved, "previous_volunteers") == [winner["user_id"], volunteers[0]["user_id"]],
                "reviewer history lost or duplicated claims")
        profile = self.action("account/get", {}, patient)
        require(field(profile, "real_name_authentication") in (True, 1), "approved patient must be certified")
        self.assert_private(profile)
        self.passed("application correction, re-submission and approval preserve audit history and certify the patient")

        # Both requests use the same owner; exactly one project may be committed.
        path, payload, method = self.route("project/create", {})
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            created = list(executor.map(lambda _: self.api.request(path, payload, patient["token"], method=method), range(2)))
        require(sorted(status for status, _ in created) == [200, 409], "concurrent create must commit exactly one project")
        project = next(value for status, value in created if status == 200)
        project_id = field(project, "project_id")
        require(isinstance(project_id, str), "project creation must return project_id")
        project_key = {"project_id": project_id}
        self.reject("project/create", {}, patient, allowed=(409,))
        self.reject("project/create", {}, donor, allowed=(403,))
        self.reject("project/get", project_key, outsider, allowed=(403, 404))
        submission = dict(project_key, donation_pool_limit="1234.56", pathography=[{
            "content": "初次病情及治疗计划", "attachments": ["https://example.test/diagnosis.pdf"]}])
        self.reject("project/submit", submission, outsider, allowed=(403,))
        self.reject("project/submit", dict(submission, donation_pool_limit="1.001"), patient, allowed=(400, 422))
        self.reject("project/submit", dict(submission, donation_pool_limit="-1.00"), patient, allowed=(400, 422))
        pending = self.action("project/submit", submission, patient)
        require(field(pending, "project_status") == 2, "first project submission must enter state 2")
        public_pending = self.action("project/list", {"limit": 100, "offset": 0}, donor)
        require(not any(field(item, "project_id") == project_id for item in public_pending),
                "unapproved project was publicly listed")
        tasks = self.action("project/tasks", {"limit": 100, "offset": 0}, volunteers[0])
        require(isinstance(tasks, list) and any(field(task, "project_id") == project_id for task in tasks),
                "submitted project must be available in volunteer tasks")
        self.reject("project/claim", project_key, donor, allowed=(403,))
        winner = self.race_claim("project/claim", project_key, volunteers)
        loser = next(account for account in volunteers if account is not winner)
        self.reject("project/review", dict(project_key, decision="approve", note="越权"), loser, allowed=(403, 409))
        self.passed("project uniqueness, ownership, decimal validation and atomic volunteer claims hold")

        self.action("project/review", dict(project_key, decision="correction", note="补充治疗费用依据"), winner)
        corrected = self.action("project/get", project_key, patient)
        require(field(corrected, "project_status") == 5, "correction must enter project state 5")
        prior_history = records(corrected, "verification_record")
        require(bool(prior_history), "project review must append history")
        revised_submission = dict(submission, donation_pool_limit="2345.67", pathography=[{
            "content": "补正后的治疗计划" + "治疗说明" * 100, "attachments": ["https://example.test/revised.pdf"]}])
        resubmitted = self.action("project/submit", revised_submission, patient)
        require(field(resubmitted, "project_status") == 6, "project re-submission must enter state 6")
        self.action("project/claim", project_key, volunteers[0])
        self.action("project/review", dict(project_key, decision="approve", note="项目审核通过"), volunteers[0])
        approved = self.action("project/get", project_key, patient)
        require(field(approved, "project_status") == 0, "approved project must have status 0")
        require(Decimal(str(field(approved, "donation_pool_limit"))) == Decimal("2345.67"), "decimal amount lost cents")
        history = records(approved, "verification_record")
        require(len(history) > len(prior_history) and all(entry in history for entry in prior_history),
                "project correction/re-submission overwrote review history")
        submissions = [event for event in history if field(event, "decision") == "submit"]
        require([field(event, "submission_no") for event in submissions] == [1, 2], "submission versions must advance")
        require([field(event, "donation_pool_limit") for event in submissions] == ["1234.56", "2345.67"],
                "review events must retain both target amounts")
        require(field(submissions[0], "pathography")[0]["content"] == "初次病情及治疗计划",
                "original materials were overwritten by correction")
        require(records(approved, "pathography")[0]["content"] == revised_submission["pathography"][0]["content"],
                "current project must display only the revised submission")
        self.passed("project correction, re-submission and approval retain complete history and decimal cents")

        public = self.action("project/list", {"limit": 100, "offset": 0}, donor)
        require(isinstance(public, list) and any(field(item, "project_id") == project_id for item in public),
                "approved project must appear in public project list")
        self.assert_private(public)
        before = records(approved, "pathography")
        entry = {"content": "治疗后病情已有改善", "attachments": ["https://example.test/follow-up.jpg"]}
        self.reject("project/pathography", dict(project_key, entry=entry), donor, allowed=(403,))
        update = dict(project_key, entry=entry, verification_record=[])
        path, payload, method = self.route("project/pathography", update)
        status, _ = self.api.request(path, payload, patient["token"], method=method)
        require(200 <= status < 300 or status in (400, 422), "unknown audit fields caused an unexpected response")
        if status >= 400:
            self.action("project/pathography", dict(project_key, entry=entry), patient)
        final = self.action("project/get", project_key, patient)
        after = records(final, "pathography")
        require(len(after) == len(before) + 1, "pathography update must append exactly one entry")
        require(all(value in after for value in before), "pathography update overwrote earlier entries")
        require(bool(field(after[-1], "time")), "pathography entry must have a server timestamp")
        require(records(final, "verification_record") == history, "pathography request overwrote audit history")
        self.assert_private(self.action("project/get", project_key, donor))
        self.passed("approved projects are public; owner-only pathography appends with server time and cannot rewrite audit history")

        # Accumulated progress is allowed to exceed 64 KiB; concurrent appends
        # must retain both entries and never rewrite a submission snapshot.
        for number in range(8):
            self.action("project/pathography", dict(project_key, entry={
                "content": "Progress {} ".format(number) + "x" * 9000, "attachments": []}), patient)
        def append_progress(number):
            return self.action("project/pathography", dict(project_key, entry={
                "content": "Concurrent progress {}".format(number), "attachments": []}), patient)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            list(executor.map(append_progress, range(2)))
        grown = self.action("project/get", project_key, patient)
        progress = records(grown, "pathography")
        require(len(progress) == len(after) + 10, "concurrent or large progress entries were lost")
        require(len(json.dumps(progress).encode()) > 65535, "fixture must exceed old pathography TEXT limit")
        require(records(grown, "verification_record") == history, "progress mutated earlier submission snapshots")
        self.assert_private(self.action("project/get", project_key, donor))
        self.passed("history and progress exceed 64 KiB safely; concurrent creation and appends preserve consistency")

        summaries = self.action("project/list", {"limit": 100, "offset": 0, "summary": True}, donor)
        summary = next(item for item in summaries if field(item, "project_id") == project_id)
        require(field(summary, "summary") == revised_submission["pathography"][0]["content"][:240],
                "cards must show at most 240 Unicode characters of the first current entry")
        require(all("pathography" not in item and "verification_record" not in item for item in summaries),
                "summary mode must not retrieve complete medical or audit history")
        self.assert_private(summaries)
        require(len(records(self.action("project/get", project_key, donor), "pathography")) == len(progress),
                "summary optimization must not truncate the detail view")
        self.passed("project cards return bounded summaries while detail and default list retain full histories")

        if self.options.dbop_token:
            headers = {"ServiceName": self.options.service_name,
                       "Authorization": self.options.dbop_token}
            direct = self.internal.ok("/workflow/account/get", {"actor_id": patient["user_id"]}, headers=headers)
            require(field(direct, "user_id") == patient["user_id"], "internal C response has the wrong identity")
            status, _ = self.internal.request("/workflow/account/get", {"actor_id": patient["user_id"]})
            require(status == 401, "internal C routes must reject requests without service authentication")
            for legacy in ("create", "update_verification_record", "update_status", "update_pathography", "update_project_volunteer"):
                status, _ = self.internal.request("/project/" + legacy, project_key, headers=headers)
                require(status == 410, "legacy project writes must not bypass normalized history")
            self.passed("C workflow routes require service authentication and return the expected account")
        self.api.ok("/api/auth/logout", token=donor["token"])
        self.api.reject("/api/me", token=donor["token"], allowed=(401,), method="GET")
        self.passed("logout revokes the bearer token")
        print("Completed {} integration groups using {} new accounts.".format(self.checks, len(self.accounts)))


def options_from_environment():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--java-url", default=os.environ.get("CROSSDO_TEST_JAVA_URL", "http://127.0.0.1:18080"))
    parser.add_argument("--dbop-url", default=os.environ.get("CROSSDO_TEST_DBOP_URL", "http://127.0.0.1:11900"))
    parser.add_argument("--timeout", type=int, default=int(os.environ.get("CROSSDO_TEST_TIMEOUT", "20")))
    parser.add_argument("--mysql-binary", default=os.environ.get("CROSSDO_TEST_MYSQL_BINARY", "mysql"))
    parser.add_argument("--mysql-database", default=os.environ.get("CROSSDO_TEST_MYSQL_DATABASE"))
    parser.add_argument("--mysql-user", default=os.environ.get("CROSSDO_TEST_MYSQL_USER", "root"))
    parser.add_argument("--mysql-socket", default=os.environ.get("CROSSDO_TEST_MYSQL_SOCKET"))
    parser.add_argument("--mysql-host", default=os.environ.get("CROSSDO_TEST_MYSQL_HOST", "127.0.0.1"))
    parser.add_argument("--mysql-port", type=int, default=int(os.environ.get("CROSSDO_TEST_MYSQL_PORT", "3306")))
    options = parser.parse_args()
    # Passwords/tokens are environment-only, keeping them out of process arguments.
    options.mysql_password = os.environ.get("CROSSDO_TEST_MYSQL_PASSWORD")
    options.dbop_token = os.environ.get("CROSSDO_TEST_DBOP_TOKEN")
    options.service_name = os.environ.get("CROSSDO_TEST_SERVICE_NAME", "CROSSDO_WEB")
    return options


def main():
    suite = Suite(options_from_environment())
    try:
        suite.run()
    except (AssertionError, ValueError, OSError, urllib.error.URLError, subprocess.SubprocessError) as error:
        # Never print payloads, tokens, passwords, SQL output or environment data.
        if isinstance(error, (Failure, FileNotFoundError)):
            message = str(error)
        else:
            message = type(error).__name__
        print("FAIL " + message, file=sys.stderr)
        if suite.accounts:
            print("New test account UUIDs: " + ", ".join(account["user_id"] for account in suite.accounts), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
