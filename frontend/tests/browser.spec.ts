import { expect, test, type Browser, type BrowserContext, type Page } from '@playwright/test';
import { randomInt, randomBytes } from 'node:crypto';
import { spawnSync } from 'node:child_process';

type TestAccount = {
  role: number;
  phone: string;
  password: string;
  name: string;
  id: string;
  context: BrowserContext;
  page: Page;
};

const appPath = (path: string) => '/#' + path;
const postResponse = (page: Page, path: string) => page.waitForResponse(
  response => response.url().endsWith(path) && response.request().method() === 'POST',
);

async function register(browser: Browser, role: number): Promise<TestAccount> {
  const context = await browser.newContext({ baseURL: process.env.CROSSDO_FRONTEND_URL || 'http://127.0.0.1:5173' });
  const page = await context.newPage();
  const phone = '19' + String(randomInt(1_000_000_000)).padStart(9, '0');
  const password = 'Browser-' + randomBytes(12).toString('hex');
  const name = '回归' + randomBytes(4).toString('hex');
  await page.goto(appPath('/register'));
  await page.getByTestId('phone-input').fill(phone);
  await page.getByTestId('password-input').fill(password);
  await page.getByTestId('name-input').fill(name);
  await page.getByTestId(`role-${role}`).click();
  const verification = postResponse(page, '/api/auth/verification-codes');
  await page.getByTestId('otp-send').click();
  const challenge = await (await verification).json();
  expect(challenge.development_code, 'Browser tests require explicit development SMS mode').toMatch(/^\d{6}$/);
  await page.getByTestId('otp-code').fill(challenge.development_code);
  const created = postResponse(page, '/api/auth/register');
  const loggedIn = postResponse(page, '/api/auth/login');
  await page.getByTestId('auth-submit').click();
  const response = await created;
  expect(response.status()).toBe(201);
  const account = await response.json();
  expect(account.user_id).toMatch(/^[0-9a-f-]{36}$/);
  expect((await loggedIn).status()).toBe(200);
  await expect(page).not.toHaveURL(/#\/(login|register)/);
  return { role, phone, password, name, id: account.user_id, context, page };
}

function certifyTestVolunteers(accounts: TestAccount[]) {
  const database = process.env.CROSSDO_TEST_MYSQL_DATABASE;
  if (!database || !/^[a-z0-9_]*test[a-z0-9_]*$/i.test(database)) {
    throw new Error('A named, isolated test database is required for volunteer certification');
  }
  const ids = accounts.map(account => {
    if (!/^[0-9a-f]{8}-[0-9a-f-]{27}$/.test(account.id)) throw new Error('Invalid test account identifier');
    return "'" + account.id + "'";
  });
  const mysql = process.env.CROSSDO_TEST_MYSQL_BINARY || 'mysql';
  const args = ['--no-defaults', '--batch', '--skip-column-names', '--database=' + database,
    '--user=' + (process.env.CROSSDO_TEST_MYSQL_USER || 'root')];
  if (process.env.CROSSDO_TEST_MYSQL_SOCKET) {
    args.push('--protocol=socket', '--socket=' + process.env.CROSSDO_TEST_MYSQL_SOCKET);
  } else {
    args.push('--protocol=tcp', '--host=' + (process.env.CROSSDO_TEST_MYSQL_HOST || '127.0.0.1'),
      '--port=' + (process.env.CROSSDO_TEST_MYSQL_PORT || '3306'));
  }
  const result = spawnSync(mysql, args, {
    input: "UPDATE `user` SET user_status=0,real_name_authentication=1 WHERE user_permission=5 AND user_id IN (" +
      ids.join(',') + "); SELECT ROW_COUNT();",
    encoding: 'utf8', timeout: 10_000,
    env: { ...process.env, ...(process.env.CROSSDO_TEST_MYSQL_PASSWORD ? { MYSQL_PWD: process.env.CROSSDO_TEST_MYSQL_PASSWORD } : {}) },
  });
  // Do not print SQL output, tokens, passwords, or the environment on failures.
  expect(result.status, 'Only newly registered volunteer UUIDs may be certified').toBe(0);
  expect(result.stdout.trim()).toBe(String(accounts.length));
}

async function submitApplication(patient: TestAccount) {
  const { page } = patient;
  await page.goto(appPath('/application'));
  await page.reload();
  await page.getByTestId('application-real-name').fill('浏览器测试患者');
  await page.getByTestId('application-id-card').fill('11010519491231002X');
  await page.getByTestId('application-age').fill('36');
  await page.getByTestId('application-material-0').fill('https://example.test/browser-medical-report.pdf');
  const saved = postResponse(page, '/api/me/application');
  await page.getByTestId('application-submit').click();
  expect((await saved).status()).toBe(200);
  await expect(page.getByTestId('application-submit')).toHaveCount(0);
}

async function claimTask(account: TestAccount, kind: 'application' | 'project', id: string) {
  const { page } = account;
  await page.goto(appPath('/tasks'));
  if (kind === 'project') await page.getByRole('tab', { name: '项目审核', exact: true }).click();
  await page.getByRole('button', { name: '刷新任务', exact: true }).click();
  const card = page.getByTestId('task-' + id);
  await expect(card).toBeVisible();
  const claimed = postResponse(page, kind === 'application' ? `/api/applications/${id}/claim` : `/api/projects/${id}/claim`);
  await card.getByRole('button', { name: '领取任务', exact: true }).click();
  expect((await claimed).status()).toBe(200);
  await expect(page).toHaveURL(new RegExp(`#/review/${kind}/${id}`));
}

async function review(account: TestAccount, kind: 'application' | 'project', id: string, decision: 'approve' | 'correction') {
  const { page } = account;
  await page.getByRole('radio', { name: decision === 'approve' ? /^审核通过/ : /^需要补正/ }).check();
  await page.getByRole('textbox', { name: /审核说明/ }).fill(decision === 'approve' ? '已核实材料，审核通过。' : '请补充医院诊断及费用证明。');
  const reviewed = postResponse(page, kind === 'application' ? `/api/applications/${id}/review` : `/api/projects/${id}/review`);
  await page.getByRole('button', { name: '提交审核结果', exact: true }).click();
  expect((await reviewed).status()).toBe(200);
}

test('患者与志愿者在浏览器完成资料补正、项目审核及病情追加', async ({ browser }) => {
  test.skip(!process.env.CROSSDO_TEST_MYSQL_DATABASE, 'Requires the isolated MySQL/Redis/DBOP/IAM test stack');
  test.setTimeout(120_000);
  const accounts: TestAccount[] = [];
  try {
    const patient = await register(browser, 3); accounts.push(patient);
    const donor = await register(browser, 2); accounts.push(donor);
    const volunteer = await register(browser, 5); accounts.push(volunteer);
    const secondVolunteer = await register(browser, 5); accounts.push(secondVolunteer);

    // A self-registered volunteer must not gain reviewer access before certification.
    await volunteer.page.goto(appPath('/tasks'));
    await expect(volunteer.page.getByTestId(/^task-/)).toHaveCount(0);
    await expect(volunteer.page.getByRole('main')).toContainText(/认证|审核/);
    certifyTestVolunteers([volunteer, secondVolunteer]);
    await volunteer.page.reload();
    await secondVolunteer.page.reload();

    await submitApplication(patient);
    await claimTask(volunteer, 'application', patient.id);
    await review(volunteer, 'application', patient.id, 'correction');
    await submitApplication(patient);
    await claimTask(secondVolunteer, 'application', patient.id);
    await review(secondVolunteer, 'application', patient.id, 'approve');

    await patient.page.goto(appPath('/application'));
    await patient.page.getByRole('button', { name: '刷新审核进度', exact: true }).click();
    await expect(patient.page.getByRole('heading', { name: '资料已通过核验', exact: true })).toBeVisible();
    await patient.page.getByRole('link', { name: '创建我的求助项目', exact: true }).click();
    const created = postResponse(patient.page, '/api/projects');
    await patient.page.getByTestId('project-create').click();
    const createdResponse = await created;
    expect(createdResponse.status()).toBe(200);
    const project = await createdResponse.json();
    const projectId = project.project_id as string;
    await patient.page.getByLabel('募款目标（元）', { exact: true }).fill('1234.56');
    await patient.page.getByLabel('病情与治疗计划', { exact: true }).fill('这是从浏览器提交的治疗计划，需要完成进一步治疗。');
    await patient.page.getByLabel('材料链接（每行一个）', { exact: true }).fill('https://example.test/browser-diagnosis.pdf');
    const submitted = postResponse(patient.page, `/api/projects/${projectId}/submit`);
    await patient.page.getByTestId('project-submit').click();
    expect((await submitted).status()).toBe(200);
    await claimTask(volunteer, 'project', projectId);
    await review(volunteer, 'project', projectId, 'approve');

    await donor.page.goto(appPath('/projects'));
    const projectCard = donor.page.getByRole('article').filter({ hasText: patient.name });
    await expect(projectCard).toBeVisible();
    await donor.page.getByLabel('搜索当前页项目', { exact: true }).fill('不存在的项目' + randomBytes(6).toString('hex'));
    await expect(donor.page.getByRole('heading', { name: '当前页还没有匹配的项目', exact: true })).toBeVisible();
    await donor.page.getByRole('button', { name: '清空搜索', exact: true }).click();
    await expect(projectCard).toBeVisible();
    await projectCard.getByRole('link', { name: '了解项目' }).click();
    await expect(donor.page).toHaveURL(new RegExp(`#/projects/${projectId}`));
    await expect(donor.page.getByRole('main')).toContainText('1,234.56');
    await expect(donor.page.getByRole('main')).toContainText('这是从浏览器提交的治疗计划');
    await expect(donor.page.getByRole('main')).not.toContainText('浏览器测试患者');
    await expect(donor.page.getByRole('main')).not.toContainText('11010519491231002X');

    await patient.page.goto(appPath('/my-project'));
    await patient.page.reload();
    await patient.page.getByLabel('新的病情进展', { exact: true }).fill('治疗后恢复良好，这是追加的病情记录。');
    await patient.page.getByLabel('进展附件（每行一个）', { exact: true }).fill('https://example.test/browser-followup.pdf');
    const appended = postResponse(patient.page, `/api/projects/${projectId}/pathography`);
    await patient.page.getByTestId('pathography-submit').click();
    expect((await appended).status()).toBe(200);
    await donor.page.reload();
    await expect(donor.page.getByRole('main')).toContainText('治疗后恢复良好');
    await expect(donor.page.getByRole('main')).toContainText('这是从浏览器提交的治疗计划');
    const loggedOut = postResponse(donor.page, '/api/auth/logout');
    await donor.page.getByRole('button', { name: '退出登录', exact: true }).click();
    expect((await loggedOut).status()).toBe(204);
    await expect(donor.page.getByRole('link', { name: '登录 / 注册' })).toBeVisible();
    await donor.page.goto(appPath('/login'));
    await donor.page.getByTestId('phone-input').fill(donor.phone);
    await donor.page.getByTestId('password-input').fill(donor.password);
    const loggedBackIn = postResponse(donor.page, '/api/auth/login');
    await donor.page.getByTestId('auth-submit').click();
    expect((await loggedBackIn).status()).toBe(200);
    await expect(donor.page.getByRole('button', { name: '退出登录', exact: true })).toBeVisible();
  } finally {
    await Promise.all(accounts.map(account => account.context.close()));
  }
});

test('移动端可导航且页面没有横向溢出', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto(appPath('/'));
  await expect(page.getByRole('main')).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
  const menu = page.getByRole('button', { name: /打开导航|菜单|切换导航/ });
  if (await menu.count()) await menu.first().click();
  await page.getByRole('link', { name: /发现项目|浏览项目|公益项目|互助项目|求助项目/ }).first().click();
  await expect(page).toHaveURL(/#\/projects/);
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
});

test('会话过期后返回登录且用户能看到服务错误', async ({ browser }) => {
  const account = await register(browser, 2);
  try {
    const { page } = account;
    await page.route('**/api/projects?*', route => route.fulfill({ status: 503, contentType: 'application/json', body: JSON.stringify({ status: 503, message: '服务暂时不可用' }) }));
    await page.goto(appPath('/projects'));
    await expect(page.getByRole('alert')).toContainText(/暂时不可用|暂不可用|加载失败/);
    await page.unroute('**/api/projects?*');
    await page.goto(appPath('/account'));
    await page.route('**/api/me', route => route.fulfill({ status: 401, contentType: 'application/json', body: JSON.stringify({ status: 401, message: '登录已过期，请重新登录' }) }));
    await page.reload();
    await expect(page).toHaveURL(/#\/login/);
    await expect(page.getByTestId('auth-form')).toBeVisible();
  } finally {
    await account.context.close();
  }
});

test('恢复登录遇到短暂服务故障时保留会话并可重试', async ({ browser }) => {
  const account = await register(browser, 2);
  try {
    const { page } = account;
    await page.goto(appPath('/account'));
    await page.route('**/api/me', route => route.fulfill({
      status: 503, contentType: 'application/json',
      body: JSON.stringify({ status: 503, message: '服务暂时不可用' }),
    }));
    await page.reload();
    await expect(page).toHaveURL(/#\/account/);
    await expect(page.getByRole('alert')).toContainText('服务暂时不可用');
    await expect(page.getByTestId('auth-form')).toHaveCount(0);
    expect(await page.evaluate(() => !!sessionStorage.getItem('crossdo.session'))).toBe(true);
    await page.unroute('**/api/me');
    await page.getByRole('button', { name: '重新恢复登录状态', exact: true }).click();
    await expect(page.getByRole('heading', { name: account.name, exact: true })).toBeVisible();
    await expect(page.getByRole('button', { name: '退出登录', exact: true })).toBeVisible();
    expect(await page.evaluate(() => !!sessionStorage.getItem('crossdo.session'))).toBe(true);
  } finally {
    await account.context.close();
  }
});

test('旧会话的延迟401不会清除重新登录后的会话', async ({ browser }) => {
  const account = await register(browser, 2);
  try {
    const { page } = account;
    let pendingRoute: import('@playwright/test').Route | undefined;
    let resolveRequested!: () => void;
    const requested = new Promise<void>(resolve => { resolveRequested = resolve; });
    await page.route('**/api/me', async route => {
      if (!pendingRoute) { pendingRoute = route; resolveRequested(); }
      else await route.continue();
    });
    await page.goto(appPath('/account'));
    await requested;
    const loggedOut = postResponse(page, '/api/auth/logout');
    await page.getByRole('button', { name: '退出登录', exact: true }).click();
    expect((await loggedOut).status()).toBe(204);
    await page.goto(appPath('/login'));
    await page.getByTestId('phone-input').fill(account.phone);
    await page.getByTestId('password-input').fill(account.password);
    const loggedIn = postResponse(page, '/api/auth/login');
    await page.getByTestId('auth-submit').click();
    expect((await loggedIn).status()).toBe(200);
    await expect(page.getByRole('button', { name: '退出登录', exact: true })).toBeVisible();
    const lateResponse = page.waitForResponse(response => response.url().endsWith('/api/me') && response.status() === 401);
    await pendingRoute!.fulfill({ status: 401, contentType: 'application/json', body: JSON.stringify({ status: 401, message: '旧登录已过期' }) });
    await (await lateResponse).finished();
    await page.unroute('**/api/me');
    await page.goto(appPath('/account'));
    await page.reload();
    await expect(page.getByRole('heading', { name: account.name, exact: true })).toBeVisible();
    expect(await page.evaluate(() => !!sessionStorage.getItem('crossdo.session'))).toBe(true);
  } finally {
    await account.context.close();
  }
});
