import { expect, test, type Browser, type BrowserContext, type Page } from '@playwright/test';
import { randomBytes, randomInt } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { mkdirSync } from 'node:fs';

interface Account { id: string; phone: string; password: string; context: BrowserContext; page: Page }
const appOrigin = process.env.CROSSDO_FRONTEND_URL || 'http://127.0.0.1:5173';
const pdf = { name: 'release-medical-proof.pdf', mimeType: 'application/pdf', buffer: Buffer.from('%PDF-1.4\n1 0 obj<</Type/Catalog>>endobj\n%%EOF') };
const responseTo = (page: Page, path: string) => page.waitForResponse(response => new URL(response.url()).pathname === path && response.request().method() === 'POST');

async function register(browser: Browser, role: number): Promise<Account> {
  const context = await browser.newContext({ baseURL: appOrigin });
  const page = await context.newPage();
  const phone = '19' + String(randomInt(1_000_000_000)).padStart(9, '0');
  const password = 'Release-' + randomBytes(12).toString('hex');
  await page.goto('/#/register');
  await page.getByTestId('name-input').fill('发布验证' + randomBytes(3).toString('hex'));
  await page.getByTestId('phone-input').fill(phone);
  await page.getByTestId('password-input').fill(password);
  await page.getByTestId(`role-${role}`).click();
  const challengeResponse = responseTo(page, '/api/auth/verification-codes');
  await page.getByTestId('otp-send').click();
  const challenge = await (await challengeResponse).json();
  expect(challenge.development_code, 'Only explicit development SMS mode may run this test').toMatch(/^\d{6}$/);
  await page.getByTestId('otp-code').fill(challenge.development_code);
  const creation = responseTo(page, '/api/auth/register');
  const login = responseTo(page, '/api/auth/login');
  await page.getByTestId('auth-submit').click();
  const created = await creation;
  expect(created.status()).toBe(201);
  const account = await created.json();
  expect((await login).status()).toBe(200);
  await expect(page).not.toHaveURL(/#\/(login|register)/);
  return { id: account.user_id, phone, password, context, page };
}

function certifyFreshVolunteer(id: string) {
  const database = process.env.CROSSDO_TEST_MYSQL_DATABASE;
  if (!database || !/^[a-z0-9_]*test[a-z0-9_]*$/i.test(database) || !/^[0-9a-f]{8}-[0-9a-f-]{27}$/.test(id)) throw new Error('Only a fresh volunteer UUID in an isolated test database may be certified');
  const args = ['--no-defaults', '--batch', '--skip-column-names', '--database=' + database, '--user=' + (process.env.CROSSDO_TEST_MYSQL_USER || 'root')];
  if (process.env.CROSSDO_TEST_MYSQL_SOCKET) args.push('--protocol=socket', '--socket=' + process.env.CROSSDO_TEST_MYSQL_SOCKET);
  else args.push('--protocol=tcp', '--host=' + (process.env.CROSSDO_TEST_MYSQL_HOST || '127.0.0.1'), '--port=' + (process.env.CROSSDO_TEST_MYSQL_PORT || '3306'));
  const result = spawnSync(process.env.CROSSDO_TEST_MYSQL_BINARY || 'mysql', args, {
    input: "UPDATE `user` SET user_status=0,real_name_authentication=1 WHERE user_permission=5 AND user_id='" + id + "'; SELECT ROW_COUNT();",
    encoding: 'utf8', timeout: 10_000,
    env: { ...process.env, ...(process.env.CROSSDO_TEST_MYSQL_PASSWORD ? { MYSQL_PWD: process.env.CROSSDO_TEST_MYSQL_PASSWORD } : {}) },
  });
  expect(result.status, 'Fresh volunteer certification must succeed').toBe(0);
  expect(result.stdout.trim()).toBe('1');
}

async function request(account: Account, method: string, path: string, data?: unknown) {
  const token = await account.page.evaluate(() => JSON.parse(sessionStorage.getItem('crossdo.session') || '{}').token);
  return account.context.request.fetch(appOrigin + path, { method, data, headers: { Authorization: `Bearer ${token}` } });
}

test('真实开发链路：验证码注册、私密证明上传、项目捐赠和退款', async ({ browser }) => {
  test.skip(!process.env.CROSSDO_TEST_MYSQL_DATABASE, 'Requires the isolated release test stack');
  test.setTimeout(150_000);
  const accounts: Account[] = [];
  try {
    const patient = await register(browser, 3); accounts.push(patient);
    const donor = await register(browser, 2); accounts.push(donor);
    const volunteer = await register(browser, 5); accounts.push(volunteer);
    expect((await (await request(donor, 'GET', '/api/payments/settings')).json()).mode).toBe('development');
    certifyFreshVolunteer(volunteer.id);

    await patient.page.goto('/#/application');
    await patient.page.getByTestId('application-real-name').fill('发布验证患者');
    await patient.page.getByTestId('application-id-card').fill('11010519491231002X');
    await patient.page.getByTestId('application-age').fill('35');
    const upload = responseTo(patient.page, '/api/files');
    await patient.page.getByTestId('upload-application').setInputFiles(pdf);
    const uploaded = await upload;
    expect(uploaded.status()).toBe(201);
    const applicationFile = await uploaded.json();
    await expect(patient.page.getByTestId('application-material-0')).toHaveValue(applicationFile.url);
    const submit = responseTo(patient.page, '/api/me/application');
    await patient.page.getByTestId('application-submit').click();
    expect((await submit).status()).toBe(200);
    expect((await request(donor, 'GET', applicationFile.url)).status()).toBe(403);
    expect((await request(volunteer, 'POST', `/api/applications/${patient.id}/claim`)).status()).toBe(200);
    expect((await request(volunteer, 'GET', applicationFile.url)).status()).toBe(200);
    expect((await request(volunteer, 'POST', `/api/applications/${patient.id}/review`, { decision: 'approve', note: '发布验证：材料齐全，审核通过。' })).status()).toBe(200);

    await patient.page.goto('/#/my-project');
    const creation = responseTo(patient.page, '/api/projects');
    await patient.page.getByTestId('project-create').click();
    const projectResponse = await creation;
    expect(projectResponse.status()).toBe(200);
    const { project_id: projectId } = await projectResponse.json();
    await patient.page.getByLabel('募款目标（元）', { exact: true }).fill('1000.00');
    await patient.page.getByLabel('病情与治疗计划').fill('发布验证：需要持续治疗，材料及资金用途会公开记录。');
    const projectUpload = responseTo(patient.page, '/api/files');
    await patient.page.getByTestId('upload-project').setInputFiles(pdf);
    const projectFile = await (await projectUpload).json();
    await expect(patient.page.getByLabel('材料链接（每行一个）')).toHaveValue(projectFile.url);
    const projectSubmit = responseTo(patient.page, `/api/projects/${projectId}/submit`);
    await patient.page.getByTestId('project-submit').click();
    expect((await projectSubmit).status()).toBe(200);
    expect((await request(volunteer, 'POST', `/api/projects/${projectId}/claim`)).status()).toBe(200);
    expect((await request(volunteer, 'POST', `/api/projects/${projectId}/review`, { decision: 'approve', note: '发布验证：项目通过审核。' })).status()).toBe(200);

    await donor.page.goto('/#/projects/' + projectId);
    await donor.page.getByTestId('donation-amount').fill('100.01');
    const orderCreation = responseTo(donor.page, '/api/donations');
    await donor.page.getByTestId('donation-create').click();
    const orderResponse = await orderCreation;
    expect(orderResponse.status()).toBe(200);
    const order = await orderResponse.json();
    expect(order.mode).toBe('development');
    const paid = responseTo(donor.page, `/api/donations/${order.id}/simulate-payment`);
    await donor.page.getByTestId('donation-simulate').click();
    expect((await paid).status()).toBe(200);
    await expect(donor.page.getByText('模拟支付已完成，没有发生真实扣款。')).toBeVisible();
    await expect(donor.page.locator('.project-funding-total')).toContainText('100.01');
    const download = donor.page.waitForEvent('download');
    await donor.page.getByRole('button', { name: /上传材料 1/ }).first().click();
    expect((await download).suggestedFilename()).toContain('release-medical-proof');

    mkdirSync('artifacts', { recursive: true });
    await donor.page.evaluate(() => window.scrollTo(0, 0));
    await donor.page.screenshot({ path: 'artifacts/release-user-project-desktop.png', fullPage: true, animations: 'disabled' });
    await donor.page.goto('/#/donations');
    await donor.page.getByRole('button', { name: '申请退款', exact: true }).click();
    await donor.page.getByLabel('退款金额（元）').fill('20.01');
    await donor.page.getByLabel('退款原因').fill('发布验证：模拟退款闭环。');
    const refundCreation = responseTo(donor.page, `/api/donations/${order.id}/refunds`);
    await donor.page.getByRole('button', { name: '提交退款申请' }).click();
    const refundResponse = await refundCreation;
    expect(refundResponse.status()).toBe(200);
    const refund = await refundResponse.json();
    const completed = responseTo(donor.page, `/api/refunds/${refund.id}/simulate-completion`);
    await donor.page.getByRole('button', { name: '完成模拟退款' }).click();
    expect((await completed).status()).toBe(200);
    await expect(donor.page.getByText('已完成', { exact: true })).toBeVisible();
    await expect(donor.page.getByText(/已退款.*20.01/)).toBeVisible();
    const funds = await (await request(donor, 'GET', `/api/projects/${projectId}/funds`)).json();
    expect(funds).toMatchObject({ received: '100.01', refunded: '20.01', balance: '80.00' });
    await donor.page.setViewportSize({ width: 390, height: 844 });
    await expect(donor.page.locator('.sidebar')).toBeHidden();
    await donor.page.evaluate(() => window.scrollTo(0, 0));
    expect(await donor.page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
    await donor.page.screenshot({ path: 'artifacts/release-user-donations-mobile.png', fullPage: true, animations: 'disabled' });
  } finally {
    await Promise.all(accounts.map(account => account.context.close()));
  }
});
