#!/usr/bin/env python3
"""Exercise account security through a running isolated Java/C/Redis stack.

Use the same CROSSDO_TEST_* environment variables as workflow_integration.py.
SMS must explicitly use development mode. This suite creates its own accounts;
the only direct SQL change promotes one of those new accounts to administrator.
Never target production. Test accounts are retained for diagnosis. No secrets,
phone numbers, verification codes, tokens, or database output are printed.
"""

import concurrent.futures
import os
import secrets
import subprocess
import sys
import threading
import urllib.error
import uuid

from workflow_integration import Failure, Suite, field, options_from_environment, require


class SecuritySuite(Suite):
    @staticmethod
    def phone():
        return "19" + "{:09d}".format(secrets.randbelow(10**9))

    @staticmethod
    def password():
        return "Security-" + secrets.token_urlsafe(18)

    def challenge(self, phone, purpose, token=None):
        result = self.api.ok("/api/auth/verification-codes",
                             {"phone_number": phone, "purpose": purpose}, token)
        require(field(result, "expires_in") == 300, "verification challenge must expire in five minutes")
        require(field(result, "resend_after") == 60, "verification resend must enforce a one-minute cooldown")
        code = field(result, "development_code")
        challenge_id = field(result, "challenge_id")
        require(isinstance(code, str) and len(code) == 6 and code.isdigit(),
                "integration requires explicitly enabled development SMS mode")
        require(str(uuid.UUID(challenge_id)) == challenge_id, "challenge identifier must be a canonical UUID")
        return {"phone_number": phone, "challenge_id": challenge_id, "code": code}

    def fresh_registration(self):
        body = self.challenge(self.phone(), "register")
        body.update(user_name="security_" + secrets.token_hex(5),
                    password=self.password(), user_permission=2)
        return body

    def save_registration(self, body):
        result = self.api.ok("/api/auth/register", body)
        user_id = field(result, "user_id")
        require(str(uuid.UUID(user_id)) == user_id, "registration must return a canonical user UUID")
        require(int(field(result, "phone_verified")) == 1, "OTP registration must mark the phone verified")
        account = {"user_id": user_id, "phone_number": body["phone_number"],
                   "password": body["password"], "user_permission": 2}
        self.accounts.append(account)
        return account

    def expect_no_content(self, path, body, token=None):
        status, result = self.api.request(path, body, token)
        require(status == 204 and result is None, path + " must return HTTP 204 after a committed change")

    def revoked(self, tokens):
        for token in tokens:
            self.api.reject("/api/me", token=token, allowed=(401,), method="GET")

    def tokens(self, account, count=2):
        return [self.login(account)["token"] for _ in range(count)]

    def bootstrap_own_admin(self, account):
        require(account in self.accounts and account["user_permission"] == 2,
                "only this suite's new donor account may be bootstrapped")
        options = self.options
        command = [options.mysql_binary, "--no-defaults", "--batch", "--skip-column-names",
                   "--database=" + options.mysql_database, "--user=" + options.mysql_user]
        if options.mysql_socket:
            command += ["--protocol=socket", "--socket=" + options.mysql_socket]
        else:
            command += ["--protocol=tcp", "--host=" + options.mysql_host,
                        "--port=" + str(options.mysql_port)]
        account_id = str(uuid.UUID(account["user_id"]))
        sql = ("UPDATE `user` SET user_permission=1,user_status=0,real_name_authentication=1,"
               "session_version=session_version+1 WHERE user_id='{}' AND user_permission=2; "
               "SELECT ROW_COUNT();").format(account_id)
        environment = os.environ.copy()
        if options.mysql_password is not None:
            environment["MYSQL_PWD"] = options.mysql_password
        result = subprocess.run(command, input=sql, text=True, capture_output=True,
                                timeout=options.timeout, env=environment)
        require(result.returncode == 0, "isolated test administrator bootstrap failed")
        require(result.stdout.strip() == "1", "bootstrap must update exactly this suite's new account")
        account["user_permission"] = 1

    def otp_checks(self):
        body = self.fresh_registration()
        unverified = {key: value for key, value in body.items() if key not in ("challenge_id", "code")}
        unverified["phone_verified"] = True
        self.api.reject("/api/auth/register", unverified, allowed=(400,))
        self.api.reject("/api/auth/verification-codes",
                        {"phone_number": body["phone_number"], "purpose": "register"}, allowed=(429,))
        self.api.reject("/api/auth/verification-codes",
                        {"phone_number": self.phone(), "purpose": "unknown"}, allowed=(400,))
        self.api.reject("/api/auth/verification-codes",
                        {"phone_number": self.phone(), "purpose": "phone_change"}, allowed=(401,))
        self.passed("SMS cooldown, purpose validation, and authenticated phone-change issuance")

        self.api.reject("/api/auth/password-reset", body, allowed=(400,))
        other_phone = dict(body, phone_number=self.phone())
        self.api.reject("/api/auth/register", other_phone, allowed=(400,))
        account = self.save_registration(body)
        self.api.reject("/api/auth/register", body, allowed=(400,))
        self.passed("verification codes bind phone and purpose, and successful registration consumes them once")

        attempts = self.fresh_registration()
        wrong = dict(attempts, code="000000" if attempts["code"] != "000000" else "111111")
        for _ in range(5):
            self.api.reject("/api/auth/register", wrong, allowed=(400,))
        self.api.reject("/api/auth/register", attempts, allowed=(400,))
        self.passed("five incorrect attempts invalidate a challenge even when the next code is correct")

        # A nonexistent phone follows the same generic recovery response without
        # creating an account. This isolates the atomic OTP-consumption property.
        racing = self.challenge(self.phone(), "password_reset")
        racing["password"] = self.password()
        barrier = threading.Barrier(2)

        def consume(_):
            barrier.wait(timeout=self.options.timeout)
            status, _ = self.api.request("/api/auth/password-reset", racing)
            return status

        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            statuses = list(executor.map(consume, range(2)))
        require(sorted(statuses) == [204, 400],
                "concurrent challenge consumption must have exactly one success; received HTTP " + str(sorted(statuses)))
        self.passed("concurrent recovery requests cannot consume one verification challenge twice")
        return account

    def reset_checks(self, account):
        prior_tokens = self.tokens(account)
        old_password = account["password"]
        challenge = self.challenge(account["phone_number"], "password_reset")
        challenge["password"] = self.password()
        self.expect_no_content("/api/auth/password-reset", challenge)
        self.api.reject("/api/auth/password-reset", challenge, allowed=(400,))
        self.revoked(prior_tokens)
        self.api.reject("/api/auth/login", {"phone_number": account["phone_number"],
                                           "password": old_password}, allowed=(401,))
        account["password"] = challenge["password"]
        self.login(account)
        self.api.ok("/api/me", token=account["token"], method="GET")
        self.passed("password recovery consumes its challenge and revokes every earlier session")
        self.passed("old passwords fail and the replacement password creates a working session")

    def phone_change_checks(self, account, other):
        prior_tokens = self.tokens(account)
        self.login(other)
        old_phone = account["phone_number"]
        challenge = self.challenge(self.phone(), "phone_change", account["token"])
        challenge["password"] = other["password"]
        self.api.reject("/api/me/phone", challenge, other["token"], allowed=(400,))
        self.api.ok("/api/me", token=other["token"], method="GET")
        challenge["password"] = "incorrect-current-password"
        self.api.reject("/api/me/phone", challenge, account["token"], allowed=(400,))
        self.api.ok("/api/me", token=account["token"], method="GET")
        self.passed("phone-change codes bind the current actor and require the current password")

        challenge["password"] = account["password"]
        self.expect_no_content("/api/me/phone", challenge, account["token"])
        self.revoked(prior_tokens)
        self.api.reject("/api/auth/login", {"phone_number": old_phone, "password": account["password"]},
                        allowed=(401,))
        account["phone_number"] = challenge["phone_number"]
        self.login(account)
        profile = self.api.ok("/api/me", token=account["token"], method="GET")
        require(field(profile, "phone_number") == account["phone_number"], "profile must show the verified replacement phone")
        require(int(field(profile, "phone_verified")) == 1, "replacement phone must remain verified")
        self.api.reject("/api/me/phone", challenge, account["token"], allowed=(400,))
        self.passed("phone change consumes the code, revokes all prior sessions, and disables old-number login")
        self.passed("the verified new number logs in; password mistakes did not consume the valid challenge")

    def freeze_checks(self, account, other, admin):
        path = "/api/admin/users/" + account["user_id"] + "/state"
        frozen = {"state": "frozen", "note": "Isolated security integration freeze"}
        self.api.reject(path, frozen, other["token"], allowed=(403,))
        self.api.reject("/api/admin/overview", token=other["token"], allowed=(403,), method="GET")
        self.api.reject("/api/admin/users/" + admin["user_id"] + "/state", frozen,
                        admin["token"], allowed=(403,))
        self.passed("ordinary accounts cannot use administration and administrators cannot freeze themselves")

        # Keep one token completely unused while frozen: otherwise checking it
        # would delete it from Redis and hide an unfreeze/session-epoch bug.
        prior_tokens = self.tokens(account, count=3)
        result = self.api.ok(path, frozen, admin["token"])
        require(field(result, "account_state") == "frozen", "freeze must persist independent account state")
        self.revoked(prior_tokens[:2])
        self.api.reject("/api/auth/login", {"phone_number": account["phone_number"],
                                           "password": account["password"]}, allowed=(403,))
        self.passed("freezing immediately rejects all existing sessions and new password logins")

        result = self.api.ok(path, {"state": "active", "note": "Isolated security integration unfreeze"}, admin["token"])
        require(field(result, "account_state") == "active", "unfreeze must restore active account state")
        self.revoked(prior_tokens)
        self.login(account)
        self.api.ok("/api/me", token=account["token"], method="GET")
        self.passed("unfreezing permits a fresh login without reviving any earlier token")

    def run(self):
        require(bool(self.options.mysql_database), "set CROSSDO_TEST_MYSQL_DATABASE to an isolated test database")
        settings = self.api.ok("/api/auth/settings", method="GET")
        require(field(settings, "sms_mode") == "development", "suite requires explicit development SMS mode")
        require(field(settings, "phone_verification_required") is True, "registration must require phone verification")
        self.passed("security settings advertise explicit development mode and required phone verification")
        account = self.otp_checks()
        other = self.save_registration(self.fresh_registration())
        admin = self.save_registration(self.fresh_registration())
        self.bootstrap_own_admin(admin)
        self.login(admin)
        self.api.ok("/api/admin/overview", token=admin["token"], method="GET")
        self.passed("only a newly created test account is promoted for administrator checks")
        self.reset_checks(account)
        self.phone_change_checks(account, other)
        self.freeze_checks(account, other, admin)
        print("Completed {} security integration groups using {} new accounts.".format(self.checks, len(self.accounts)))


def main():
    suite = SecuritySuite(options_from_environment())
    try:
        suite.run()
    except (AssertionError, ValueError, OSError, urllib.error.URLError, subprocess.SubprocessError) as error:
        message = str(error) if isinstance(error, (Failure, FileNotFoundError)) else type(error).__name__
        print("FAIL " + message, file=sys.stderr)
        if suite.accounts:
            print("New test account UUIDs: " + ", ".join(account["user_id"] for account in suite.accounts), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
