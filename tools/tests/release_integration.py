#!/usr/bin/env python3
"""Exercise release operations, private files and the financial lifecycle on an isolated stack.

Only newly registered test users are changed directly (one administrator bootstrap).
All review, financial and closure transitions run through the actual Java/C APIs.
"""
import base64
import concurrent.futures
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request
import uuid
from decimal import Decimal

from workflow_integration import Suite, Failure, field, require, options_from_environment


PNG = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+j6YQAAAAASUVORK5CYII=')


class ReleaseSuite(Suite):
    def sql(self, query):
        o = self.options
        require(bool(o.mysql_database) and 'test' in o.mysql_database.lower(), 'an isolated test database is required')
        cmd = [o.mysql_binary, '--batch', '--skip-column-names', '--user=' + o.mysql_user, '--database=' + o.mysql_database]
        cmd += ['--protocol=socket', '--socket=' + o.mysql_socket] if o.mysql_socket else [
            '--protocol=tcp', '--host=' + o.mysql_host, '--port=' + str(o.mysql_port)]
        env = os.environ.copy()
        if o.mysql_password is not None:
            env['MYSQL_PWD'] = o.mysql_password
        result = subprocess.run(cmd, input=query, text=True, capture_output=True, timeout=o.timeout, env=env)
        require(result.returncode == 0, 'test fixture SQL failed')
        return result.stdout.strip()

    def upload(self, account, purpose, project=None):
        boundary = 'crossdo-' + uuid.uuid4().hex
        parts = []
        for name, value in dict(purpose=purpose, **({'project_id': project} if project else {})).items():
            parts.append(('--' + boundary + '\r\nContent-Disposition: form-data; name="' + name + '"\r\n\r\n' + value + '\r\n').encode())
        parts.append(('--' + boundary + '\r\nContent-Disposition: form-data; name="file"; filename="test.png"\r\nContent-Type: image/png\r\n\r\n').encode() + PNG + b'\r\n')
        parts.append(('--' + boundary + '--\r\n').encode())
        req = urllib.request.Request(self.options.java_url + '/api/files', data=b''.join(parts), headers={
            'Authorization': 'Bearer ' + account['token'], 'Content-Type': 'multipart/form-data; boundary=' + boundary})
        try:
            with urllib.request.urlopen(req, timeout=self.options.timeout) as response:
                require(response.status == 201, 'upload must return 201')
                return json.load(response)
        except urllib.error.HTTPError as exc:
            raise Failure('file upload failed HTTP ' + str(exc.code)) from None

    def download(self, url, account=None, allowed=(200,)):
        headers = {'Authorization': 'Bearer ' + account['token']} if account else {}
        req = urllib.request.Request(self.options.java_url + url, headers=headers)
        try:
            response = urllib.request.urlopen(req, timeout=self.options.timeout)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            require(response.status in allowed, 'file access expected {}, received {}'.format(allowed, response.status))
            if response.status == 200:
                require(response.read() == PNG, 'file bytes must round trip intact')
                require(response.headers.get('X-Content-Type-Options') == 'nosniff', 'file download must disable MIME sniffing')

    def run(self):
        api = self.api
        patient, donor, other = [self.login(self.register(role)) for role in (3, 2, 2)]
        admin = self.register(2)
        self.sql("UPDATE user SET user_permission=1,user_status=0,real_name_authentication=1,session_version=session_version+1 WHERE user_id='{}';".format(str(uuid.UUID(admin['user_id']))))
        self.login(admin)
        reviewers = [self.login(self.register(5)) for _ in range(2)]
        for reviewer in reviewers:
            api.ok('/api/admin/users/' + reviewer['user_id'] + '/certification', {'approved': True, 'note': '集成测试认证'}, admin['token'])
            self.login(reviewer)
        api.reject('/api/admin/overview', token=donor['token'], method='GET', allowed=(403,))
        require(isinstance(api.ok('/api/admin/overview', token=admin['token'], method='GET'), dict), 'admin overview')
        self.passed('administrator certification and non-admin authorization')

        application = self.upload(patient, 'application')['url']
        self.download(application, patient)
        self.download(application, allowed=(403,))
        self.download(application, reviewers[0], allowed=(403,))
        self.action('application/submit', dict(self.application_payload(), materials=[application]), patient)
        key = {'user_id': patient['user_id']}
        self.action('application/claim', key, reviewers[0])
        self.download(application, reviewers[0])
        api.ok('/api/admin/tasks/application/' + patient['user_id'] + '/reassign', {
            'volunteer_id': reviewers[1]['user_id'], 'note': '任务移交测试'}, admin['token'])
        self.download(application, reviewers[0], allowed=(403,))
        self.download(application, reviewers[1])
        self.action('application/review', dict(key, decision='approve', note='资料核实'), reviewers[1])
        self.download(application, allowed=(403,))
        self.passed('uploaded application is private and reassignment revokes old reviewer access')

        project = self.action('project/create', {}, patient)
        pid = project['project_id']
        project_url = '/api/projects/' + pid
        previous = self.upload(patient, 'project', pid)['url']
        self.action('project/submit', {'project_id': pid, 'donation_pool_limit': '1000.00', 'pathography': [
            {'content': '原始材料', 'attachments': [previous]}]}, patient)
        self.action('project/claim', {'project_id': pid}, reviewers[0])
        self.action('project/review', {'project_id': pid, 'decision': 'correction', 'note': '补充资料'}, reviewers[0])
        current = self.upload(patient, 'project', pid)['url']
        self.action('project/submit', {'project_id': pid, 'donation_pool_limit': '1000.00', 'pathography': [
            {'content': '核实后的公开治疗计划', 'attachments': [current]}]}, patient)
        self.download(current, allowed=(403,))
        self.action('project/claim', {'project_id': pid}, reviewers[1])
        self.action('project/review', {'project_id': pid, 'decision': 'approve', 'note': '公开审核通过'}, reviewers[1])
        self.download(current)
        self.download(previous, allowed=(403,))
        self.download(current, {'token': 'invalid-token'}, allowed=(401,))
        self.passed('only current published project files are public; old versions and invalid tokens are rejected')

        notices = api.ok('/api/notifications', token=patient['token'], method='GET')
        require(len(notices) >= 3, 'workflow transitions notify the owner')
        notice_id = notices[0]['id']
        api.reject('/api/notifications/' + notice_id + '/read', token=other['token'], allowed=(403, 404))
        api.ok('/api/notifications/' + notice_id + '/read', token=patient['token'])
        api.ok('/api/notifications/read-all', token=patient['token'])
        require(api.ok('/api/notifications/unread-count', token=patient['token'], method='GET')['count'] == 0, 'all notifications marked read')
        self.passed('notifications are recipient-scoped and read counts stay consistent')

        body = {'project_id': pid, 'amount': '100.01', 'idempotency_key': str(uuid.uuid4())}
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            orders = list(pool.map(lambda _: api.ok('/api/donations', body, donor['token']), range(2)))
        require(orders[0]['id'] == orders[1]['id'], 'concurrent idempotent donation creates one order')
        oid = orders[0]['id']
        api.reject('/api/donations', dict(body, amount='100.02'), donor['token'], allowed=(409,))
        api.reject('/api/donations/' + oid, token=other['token'], method='GET', allowed=(403,))
        api.reject('/api/donations/' + oid + '/simulate-payment', token=other['token'], allowed=(403,))
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            list(pool.map(lambda _: api.ok('/api/donations/' + oid + '/simulate-payment', token=donor['token']), range(2)))
        funds = api.ok(project_url + '/funds', method='GET')
        require(funds['balance'] == '100.01' and funds['mode'] == 'development' and 'reserved' not in funds, 'anonymous funds expose exact balance and mode without private reservation')
        api.reject('/api/payments/callback', {'event_id': 'forged'}, allowed=(400, 401, 403, 503))
        self.passed('donation and concurrent callbacks are idempotent; financial privacy and callback authentication hold')

        refund_body = {'amount': '30.01', 'reason': '部分退款', 'idempotency_key': str(uuid.uuid4())}
        refund = api.ok('/api/donations/' + oid + '/refunds', refund_body, donor['token'])
        require(api.ok('/api/donations/' + oid + '/refunds', refund_body, donor['token'])['id'] == refund['id'], 'refund retry must reuse request')
        funds = api.ok(project_url + '/funds', token=patient['token'], method='GET')
        require(funds['reserved'] == '30.01', 'refund reserves funds immediately')
        api.reject('/api/donations/' + oid + '/refunds', dict(refund_body, amount='71.00', idempotency_key=str(uuid.uuid4())), donor['token'], allowed=(409,))
        for _ in range(2):
            api.ok('/api/refunds/' + refund['id'] + '/simulate-completion', token=donor['token'])
        require(api.ok(project_url + '/funds', method='GET')['balance'] == '70.00', 'refund debit occurs once')

        disbursement_body = {'amount': '40.00', 'purpose': '治疗费用', 'evidence': [current],
                             'payout_reference': 'DEVELOPMENT-ONLY', 'idempotency_key': str(uuid.uuid4())}
        payout = api.ok(project_url + '/disbursements', disbursement_body, patient['token'])
        require(api.ok(project_url + '/disbursements', disbursement_body, patient['token'])['id'] == payout['id'], 'payout creation is idempotent')
        review_url = '/api/admin/disbursements/' + payout['id'] + '/review'
        api.reject(review_url, {'decision': 'approve', 'note': '越权'}, donor['token'], allowed=(403,))
        api.reject('/api/disbursements/' + payout['id'] + '/simulate-completion', token=admin['token'], allowed=(409,))
        api.ok(review_url, {'decision': 'approve', 'note': '费用已核实'}, admin['token'])
        for _ in range(2):
            api.ok('/api/disbursements/' + payout['id'] + '/simulate-completion', token=admin['token'])
        funds = api.ok(project_url + '/funds', method='GET')
        require(funds['balance'] == '30.00' and funds['used'] == '40.00' and len(funds['usage']) == 1, 'payout debit and public usage appear exactly once')
        require('payout_reference' not in funds['usage'][0], 'public usage must not disclose payout account')
        self.passed('partial refund and approved disbursement reserve and debit exact amounts once')

        case_file = self.upload(donor, 'case')['url']
        report = api.ok('/api/cases', {'kind': 'report', 'project_id': pid, 'reason': '核实公开内容', 'evidence': [case_file]}, donor['token'])
        cid = report['id']
        target_view = api.ok('/api/cases/' + cid, token=patient['token'], method='GET')
        require(target_view['evidence'] == [] and target_view['evidence_private'], 'report evidence stays private from target')
        api.reject('/api/cases/' + cid, token=other['token'], method='GET', allowed=(403,))
        self.download(case_file, patient, allowed=(403,))
        self.download(case_file, admin)
        api.ok('/api/admin/cases/' + cid + '/resolve', {'decision': 'resolved', 'note': '已完成核实'}, admin['token'])
        appeal = api.ok('/api/cases', {'kind': 'appeal', 'parent_id': cid, 'reason': '补充说明申请复核', 'evidence': []}, patient['token'])
        require(appeal['parent_id'] == cid, 'appeal must retain parent case')
        api.reject('/api/cases', {'kind': 'appeal', 'parent_id': cid, 'reason': '重复申诉', 'evidence': []}, patient['token'], allowed=(409,))
        self.passed('report evidence privacy, administrator resolution and linked appeals')

        api.ok(project_url + '/closure', {'reason': '治疗阶段结束', 'remaining_plan': 'refund'}, patient['token'])
        closure_review = '/api/admin/projects/' + pid + '/closure/review'
        decision = {'decision': 'approve', 'note': '资金结清后结项'}
        api.reject(closure_review, decision, admin['token'], allowed=(409,))
        api.reject('/api/donations', dict(body, idempotency_key=str(uuid.uuid4())), donor['token'], allowed=(409,))
        final_refund = api.ok('/api/donations/' + oid + '/refunds', {
            'amount': '30.00', 'reason': '退回结项剩余款', 'idempotency_key': str(uuid.uuid4())}, admin['token'])
        api.ok('/api/refunds/' + final_refund['id'] + '/simulate-completion', token=admin['token'])
        api.ok(closure_review, decision, admin['token'])
        closed = self.action('project/get', {'project_id': pid}, donor)
        require(closed['project_status'] == 13, 'settled project reaches closed state')
        closure = api.ok(project_url + '/closure', method='GET')
        require(closure['status'] == 'approved' and 'decision_note' not in closure and 'reviewed_by' not in closure, 'public closure redacts internal review')
        profile = self.action('account/get', {}, patient)
        require(not profile.get('project_id') and profile['existing_projects'] == 0, 'closed project releases owner project slot')
        self.download(current)
        self.action('project/create', {}, patient)
        self.passed('closure blocks unsettled funds, refunds remaining balance and releases the project slot')

        reconciliation = api.ok('/api/admin/finance/reconciliation?limit=100', token=admin['token'], method='GET')
        item = next(x for x in reconciliation if x['project_id'] == pid)
        require(item['matches'] and Decimal(item['balance']) == 0 and Decimal(item['reserved']) == 0, 'ledger/cache/account reconcile to zero')
        result = self.sql("SELECT COUNT(*),SUM(amount) FROM fund_ledger WHERE project_id='{}';".format(pid))
        require(result == '4\t0.00', 'exactly four immutable entries net to zero')
        if self.options.dbop_token:
            headers = {'ServiceName': self.options.service_name, 'Authorization': self.options.dbop_token}
            for path in ('/user/add', '/ledger/create_donation'):
                status, _ = self.internal.request(path, {}, headers=headers)
                require(status == 410, 'legacy write routes must stay retired')
        self.passed('financial journal, cached totals and account balance reconcile; legacy write bypasses stay closed')
        print('Completed {} release integration groups using {} new accounts.'.format(self.checks, len(self.accounts)))


if __name__ == '__main__':
    try:
        ReleaseSuite(options_from_environment()).run()
    except (AssertionError, ValueError, OSError, urllib.error.URLError, subprocess.SubprocessError) as error:
        print('FAIL ' + (str(error) if isinstance(error, Failure) else type(error).__name__), file=sys.stderr)
        sys.exit(1)
