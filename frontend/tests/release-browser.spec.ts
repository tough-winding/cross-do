import { expect, test, type APIRequestContext, type Page } from '@playwright/test';
import { randomBytes, randomInt } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { mkdir } from 'node:fs/promises';

type Account = { id: string; phone: string; password: string; name: string };
const frontendUrl = process.env.CROSSDO_FRONTEND_URL || 'http://127.0.0.1:5173';
const artifacts = process.env.CROSSDO_ARTIFACT_DIR || '/workspace/cross-do-artifacts';

async function register(request: APIRequestContext, role = 2): Promise<Account> {
  const phone = '19' + String(randomInt(1_000_000_000)).padStart(9, '0');
  const password = 'Release-' + randomBytes(12).toString('hex');
  const name = '运营回归' + randomBytes(4).toString('hex');
  const challengeResponse = await request.post('/api/auth/verification-codes', { data: { phone_number: phone, purpose: 'register' } });
  expect(challengeResponse.status()).toBe(200);
  const challenge = await challengeResponse.json();
  expect(challenge.development_code, 'Release browser tests require explicit development SMS mode').toMatch(/^\d{6}$/);
  const response = await request.post('/api/auth/register', { data: { phone_number: phone, user_name: name, password, user_permission: role, challenge_id: challenge.challenge_id, code: challenge.development_code } });
  expect(response.status()).toBe(201);
  const account = await response.json();
  expect(account.user_id).toMatch(/^[0-9a-f-]{36}$/);
  return { id: account.user_id, phone, password, name };
}

function bootstrapTestAdmin(account: Account) {
  const database = process.env.CROSSDO_TEST_MYSQL_DATABASE;
  if (!database || !/^[a-z0-9_]*test[a-z0-9_]*$/i.test(database)) throw new Error('A named isolated test database is required to bootstrap this test administrator');
  if (!/^[0-9a-f]{8}-[0-9a-f-]{27}$/.test(account.id)) throw new Error('Invalid test account identifier');
  const args = ['--no-defaults', '--batch', '--skip-column-names', '--database=' + database, '--user=' + (process.env.CROSSDO_TEST_MYSQL_USER || 'root')];
  if (process.env.CROSSDO_TEST_MYSQL_SOCKET) args.push('--protocol=socket', '--socket=' + process.env.CROSSDO_TEST_MYSQL_SOCKET);
  else args.push('--protocol=tcp', '--host=' + (process.env.CROSSDO_TEST_MYSQL_HOST || '127.0.0.1'), '--port=' + (process.env.CROSSDO_TEST_MYSQL_PORT || '3306'));
  const result = spawnSync(process.env.CROSSDO_TEST_MYSQL_BINARY || 'mysql', args, {
    input: "UPDATE `user` SET user_permission=1,user_status=0,real_name_authentication=1,session_version=session_version+1 WHERE user_permission=2 AND user_id='" + account.id + "'; SELECT ROW_COUNT();",
    encoding: 'utf8', timeout: 10_000, env: { ...process.env, ...(process.env.CROSSDO_TEST_MYSQL_PASSWORD ? { MYSQL_PWD: process.env.CROSSDO_TEST_MYSQL_PASSWORD } : {}) },
  });
  expect(result.status, 'Only this newly registered test account may become an administrator').toBe(0);
  expect(result.stdout.trim()).toBe('1');
}

async function login(page: Page, account: Account): Promise<string> {
  await page.goto('/#/login');
  await page.getByTestId('phone-input').fill(account.phone);
  await page.getByTestId('password-input').fill(account.password);
  const response = page.waitForResponse(value => value.url().endsWith('/api/auth/login') && value.request().method() === 'POST');
  await page.getByTestId('auth-submit').click();
  const signedIn = await response;
  expect(signedIn.status()).toBe(200);
  await expect(page.getByRole('button', { name: '退出登录', exact: true })).toBeVisible();
  return (await signedIn.json()).token;
}

test('真实接口：认证志愿者、处理私密举报、通知已读与结果申诉', async ({ browser, request }) => {
  test.setTimeout(120_000);
  const reporter = await register(request);
  const target = await register(request);
  const admin = await register(request);
  const volunteer = await register(request, 5);
  bootstrapTestAdmin(admin);
  const reporterContext = await browser.newContext({ baseURL: frontendUrl, viewport: { width: 390, height: 844 } });
  const adminContext = await browser.newContext({ baseURL: frontendUrl, viewport: { width: 1440, height: 1000 } });
  const reporterPage = await reporterContext.newPage();
  const adminPage = await adminContext.newPage();
  try {
    await mkdir(artifacts, { recursive: true });
    const reporterToken = await login(reporterPage, reporter);
    const reason = '请核实本次浏览器测试的资料记录。编号：' + randomBytes(5).toString('hex');
    const decisionNote = '已核实本次测试提交的材料，处理记录已保存。';
    await reporterPage.goto('/#/cases?target_user_id=' + target.id);
    await reporterPage.getByLabel('具体情况').fill(reason);
    const uploadResponse = reporterPage.waitForResponse(value => value.url().endsWith('/api/files') && value.request().method() === 'POST');
    await reporterPage.getByTestId('upload-case').setInputFiles({ name: 'release-evidence.pdf', mimeType: 'application/pdf', buffer: Buffer.from('%PDF-1.4\n1 0 obj\n<< /Type /Catalog >>\nendobj\ntrailer\n<< /Root 1 0 R >>\n%%EOF') });
    const uploaded = await uploadResponse;
    expect(uploaded.status()).toBe(201);
    const evidence = await uploaded.json();
    await expect(reporterPage.getByLabel('补充材料链接（可选）')).toHaveValue(evidence.url);
    const createResponse = reporterPage.waitForResponse(value => value.url().endsWith('/api/cases') && value.request().method() === 'POST');
    await reporterPage.locator('#case-compose').getByRole('button', { name: '提交举报', exact: true }).click();
    const created = await createResponse;
    expect(created.status(), await created.text()).toBe(200);
    const report = await created.json();
    expect(report.evidence).toEqual([evidence.url]);
    await expect(reporterPage.getByRole('heading', { name: '举报详情' })).toBeVisible();
    expect(await reporterPage.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
    await reporterPage.screenshot({ path: artifacts + '/release-cases-mobile.png', fullPage: true });

    const adminToken = await login(adminPage, admin);
    await adminPage.goto('/#/admin?tab=users');
    await adminPage.getByLabel('搜索账户').fill(volunteer.name);
    await adminPage.getByRole('button', { name: '搜索', exact: true }).click();
    await expect(adminPage.getByRole('heading', { name: volunteer.name, exact: true })).toBeVisible();
    await adminPage.getByRole('button', { name: '处理志愿者认证' }).click();
    await adminPage.getByLabel('处理说明').fill('浏览器回归已核实本测试志愿者资料。');
    const certification = adminPage.waitForResponse(value => value.url().endsWith('/api/admin/users/' + volunteer.id + '/certification') && value.request().method() === 'POST');
    await adminPage.getByRole('button', { name: '保存认证结论' }).click();
    expect((await certification).status()).toBe(200);
    await expect(adminPage.getByText('资料认证：已通过', { exact: false })).toBeVisible();

    await adminPage.goto('/#/admin?tab=cases');
    await adminPage.locator('article.ops-card').filter({ hasText: reason }).getByRole('button', { name: '查看并处理' }).click();
    await expect(adminPage.getByRole('heading', { name: '举报详情' })).toBeVisible();
    expect((await request.get(evidence.url, { headers: { Authorization: 'Bearer ' + adminToken } })).status()).toBe(200);
    await adminPage.screenshot({ path: artifacts + '/release-admin-desktop.png', fullPage: true });
    await adminPage.getByLabel('处理说明').fill(decisionNote);
    const resolveResponse = adminPage.waitForResponse(value => value.url().endsWith('/api/admin/cases/' + report.id + '/resolve') && value.request().method() === 'POST');
    await adminPage.getByRole('button', { name: '保存处理结果' }).click();
    expect((await resolveResponse).status()).toBe(200);
    await expect(adminPage.getByText('处理结果已保存并通知相关人员。')).toBeVisible();

    const targetLogin = await request.post('/api/auth/login', { data: { phone_number: target.phone, password: target.password } });
    expect(targetLogin.status()).toBe(200);
    const targetToken = (await targetLogin.json()).token;
    expect((await request.get(evidence.url, { headers: { Authorization: 'Bearer ' + targetToken } })).status()).toBe(403);
    const targetCase = await request.get('/api/cases/' + report.id, { headers: { Authorization: 'Bearer ' + targetToken } });
    expect(targetCase.status()).toBe(200);
    expect((await targetCase.json()).evidence).toEqual([]);

    await reporterPage.goto('/#/notifications');
    const notification = reporterPage.locator('article.ops-card').filter({ hasText: decisionNote });
    await expect(notification).toBeVisible();
    await notification.getByRole('button', { name: '标为已读', exact: true }).click();
    await expect(notification.getByText('已读', { exact: true })).toBeVisible();
    await notification.getByRole('link', { name: '查看相关进展' }).click();
    await expect(reporterPage.getByRole('heading', { name: '举报详情' })).toBeVisible();
    await reporterPage.getByRole('button', { name: '提出申诉' }).click();
    await reporterPage.getByLabel('申诉理由').fill('补充申诉说明，请结合本次新说明重新核实。');
    const appealResponse = reporterPage.waitForResponse(value => value.url().endsWith('/api/cases') && value.request().method() === 'POST');
    await reporterPage.getByRole('button', { name: '提交申诉', exact: true }).click();
    const appealed = await appealResponse;
    expect(appealed.status()).toBe(200);
    const appeal = await appealed.json();
    expect(appeal.parent_id).toBe(report.id);
    expect(appeal.target_user_id).toBe(target.id);
    const notifications = await request.get('/api/notifications?limit=100', { headers: { Authorization: 'Bearer ' + reporterToken } });
    expect(notifications.status()).toBe(200);
    expect((await notifications.json()).some((item: { body: string; read_at: string | null }) => item.body === decisionNote && !!item.read_at)).toBe(true);
  } finally {
    await reporterContext.close();
    await adminContext.close();
  }
});
