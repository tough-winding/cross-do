import { expect, test, type Page } from '@playwright/test';

const userId = '60000000-0000-4000-8000-000000000001';
const projectId = '70000000-0000-4000-8000-000000000001';
const orderId = '80000000-0000-4000-8000-000000000001';
const fileId = '90000000-0000-4000-8000-000000000001';
const timestamp = '2026-11-06 10:00:00';

async function projectSession(page: Page) {
  await page.addInitScript(() => sessionStorage.setItem('crossdo.session', JSON.stringify({ token: 'user-ui-test-session', expiresAt: Date.now() + 3_600_000 })));
  await page.route('**/api/**', async route => {
    const path = new URL(route.request().url()).pathname;
    if (path === '/api/me') return route.fulfill({ json: { user_id: userId, user_name: '捐助者', user_permission: 2, user_status: 0, real_name_authentication: 1, phone_number: '19000000000' } });
    if (path === '/api/notifications/unread-count') return route.fulfill({ json: { count: 0 } });
    if (path === '/api/payments/settings') return route.fulfill({ json: { mode: 'development' } });
    if (path === `/api/projects/${projectId}`) return route.fulfill({ json: { project_id: projectId, user_id: fileId, user_name: '求助者', user_age: 35, project_status: 0, project_create_time: timestamp, last_update_time: timestamp, donation_pool_limit: '10000.00', current_donations_received: '0.00', current_used_donations: '0.00', pathography: [{ content: '治疗记录', time: timestamp, attachments: ['/api/files/' + fileId] }] } });
    if (path === `/api/projects/${projectId}/funds`) return route.fulfill({ json: { project_id: projectId, received: '0.00', used: '0.00', refunded: '0.00', balance: '0.00', usage: [] } });
    return route.fulfill({ status: 404, json: { message: '测试未配置接口' } });
  });
}

test('捐赠超时后刷新页面仍复用幂等编号，成功后新订单使用新编号', async ({ page }) => {
  await projectSession(page);
  const requests: { project_id: string; amount: string; idempotency_key: string }[] = [];
  await page.route('**/api/donations', route => {
    requests.push(route.request().postDataJSON());
    return requests.length === 1
      ? route.fulfill({ status: 500, json: { message: 'Simulated uncertain response' } })
      : route.fulfill({ status: 201, json: { id: orderId, project_id: projectId, donor_id: userId, amount: '100.01', currency: 'CNY', status: 'pending', refunded_amount: '0.00', refund_reserved: '0.00', mode: 'development', checkout_url: null, created_at: timestamp, refunds: [] } });
  });
  await page.goto('/#/projects/' + projectId);
  await page.getByTestId('donation-amount').fill('100.01');
  await page.getByTestId('donation-create').click();
  await expect(page.getByText('服务暂时不可用，请稍后重试。')).toBeVisible();
  await page.reload();
  await page.getByTestId('donation-amount').fill('100.01');
  await page.getByTestId('donation-create').click();
  await expect(page.getByTestId('donation-simulate')).toBeVisible();
  expect(requests).toHaveLength(2);
  expect(requests[1]).toEqual(requests[0]);
  await page.getByTestId('donation-amount').fill('100.01');
  await page.getByTestId('donation-create').click();
  await expect.poll(() => requests.length).toBe(3);
  expect(requests[2].idempotency_key).not.toBe(requests[0].idempotency_key);
});

test('项目文件以鉴权请求下载，令牌不进入地址', async ({ page }) => {
  await projectSession(page);
  let authorization = '';
  let requestedUrl = '';
  await page.route('**/api/files/' + fileId, route => {
    authorization = route.request().headers().authorization;
    requestedUrl = route.request().url();
    return route.fulfill({ status: 200, contentType: 'application/pdf', headers: { 'Content-Disposition': 'attachment; filename="report.pdf"' }, body: '%PDF-1.4\n%%EOF' });
  });
  await page.goto('/#/projects/' + projectId);
  const download = page.waitForEvent('download');
  await page.getByRole('button', { name: /上传材料 1/ }).click();
  expect((await download).suggestedFilename()).toBe('report.pdf');
  expect(authorization).toBe('Bearer user-ui-test-session');
  expect(new URL(requestedUrl).search).toBe('');
});

test('找回密码更换手机号会清除旧验证码，开发验证码由用户填写', async ({ page }) => {
  let requests = 0;
  let reset: Record<string, unknown> | null = null;
  await page.route('**/api/auth/settings', route => route.fulfill({ json: { sms_mode: 'development', phone_verification_required: true } }));
  await page.route('**/api/auth/verification-codes', route => {
    requests++;
    return route.fulfill({ json: { challenge_id: `challenge-${requests}`, expires_in: 300, resend_after: 60, development_code: requests === 1 ? '111111' : '222222' } });
  });
  await page.route('**/api/auth/password-reset', route => { reset = route.request().postDataJSON(); return route.fulfill({ status: 204 }); });
  await page.goto('/#/recover');
  await page.getByLabel('注册手机号').fill('19000000001');
  await page.getByTestId('otp-send').click();
  await expect(page.getByTestId('development-code')).toHaveText('111111');
  await expect(page.getByTestId('otp-code')).toHaveValue('');
  await page.getByTestId('otp-code').fill('111111');
  await page.getByLabel('注册手机号').fill('19000000002');
  await expect(page.getByTestId('otp-code')).toHaveValue('');
  await expect(page.getByTestId('development-code')).toHaveCount(0);
  await page.getByTestId('otp-send').click();
  await expect(page.getByTestId('development-code')).toHaveText('222222');
  await page.getByTestId('otp-code').fill('222222');
  await page.getByLabel('新密码', { exact: true }).fill('Recovery-password-1');
  await page.getByLabel('再次填写新密码').fill('Recovery-password-1');
  await page.getByRole('button', { name: '更新密码' }).click();
  await expect(page.getByText('密码已更新，请使用新密码重新登录。')).toBeVisible();
  expect(reset).toEqual({ phone_number: '19000000002', challenge_id: 'challenge-2', code: '222222', password: 'Recovery-password-1' });
});

test('更换手机号成功后清除会话并引导使用新手机号登录', async ({ page }) => {
  await projectSession(page);
  let changed: Record<string, unknown> | null = null;
  await page.route('**/api/auth/settings', route => route.fulfill({ json: { sms_mode: 'development', phone_verification_required: true } }));
  await page.route('**/api/auth/verification-codes', route => {
    expect(route.request().headers().authorization).toBe('Bearer user-ui-test-session');
    expect(route.request().postDataJSON().purpose).toBe('phone_change');
    return route.fulfill({ json: { challenge_id: 'phone-change-challenge', expires_in: 300, resend_after: 60, development_code: '333333' } });
  });
  await page.route('**/api/me/phone', route => { changed = route.request().postDataJSON(); return route.fulfill({ status: 204 }); });
  await page.route('**/api/auth/logout', route => route.fulfill({ status: 204 }));
  await page.goto('/#/account');
  await page.getByText('更换注册手机号', { exact: true }).click();
  await page.getByLabel('新手机号', { exact: true }).fill('19000000003');
  await page.getByTestId('otp-send').click();
  await expect(page.getByTestId('development-code')).toHaveText('333333');
  await page.getByTestId('otp-code').fill('333333');
  await page.getByLabel('当前登录密码').fill('Current-password-1');
  await page.getByRole('button', { name: '验证并更换手机号' }).click();
  await expect(page).toHaveURL(/#\/login(?:\?|$)/);
  await expect(page.getByText('手机号已更新，请使用新手机号和原密码重新登录。')).toBeVisible();
  expect(await page.evaluate(() => sessionStorage.getItem('crossdo.session'))).toBeNull();
  expect(changed).toEqual({ phone_number: '19000000003', challenge_id: 'phone-change-challenge', code: '333333', password: 'Current-password-1' });
});

test('患者可上传用款证明并提交结项原因，结项后关闭申请入口', async ({ page }) => {
  await projectSession(page);
  let closed = false;
  let disbursement: Record<string, unknown> | null = null;
  let closure: Record<string, unknown> | null = null;
  await page.route('**/api/me', route => route.fulfill({ json: { user_id: userId, user_name: '项目申请人', user_permission: 3, user_status: 0, real_name_authentication: 1, project_id: projectId, phone_number: '19000000000' } }));
  await page.route('**/api/projects/' + projectId, route => route.fulfill({ json: { project_id: projectId, user_id: userId, user_name: '项目申请人', user_age: 35, project_status: closed ? 12 : 0, project_create_time: timestamp, last_update_time: timestamp, donation_pool_limit: '10000.00', current_donations_received: '100.00', current_used_donations: '0.00', pathography: [{ content: '治疗记录', time: timestamp, attachments: [] }] } }));
  await page.route('**/api/projects/' + projectId + '/funds', route => route.fulfill({ json: { project_id: projectId, received: '100.00', used: '0.00', refunded: '0.00', balance: '100.00', reserved: disbursement ? '10.01' : '0.00', usage: [] } }));
  await page.route('**/api/projects/' + projectId + '/disbursements', route => {
    if (route.request().method() === 'POST') {
      disbursement = route.request().postDataJSON();
      return route.fulfill({ json: { id: orderId, project_id: projectId, ...disbursement, mode: 'development', status: 'requested', created_at: timestamp } });
    }
    return route.fulfill({ json: disbursement ? [{ id: orderId, project_id: projectId, ...disbursement, mode: 'development', status: 'requested', created_at: timestamp }] : [] });
  });
  await page.route('**/api/files', route => route.fulfill({ status: 201, json: { id: fileId, name: 'expense.pdf', size: 15, mime_type: 'application/pdf', url: '/api/files/' + fileId } }));
  await page.route('**/api/projects/' + projectId + '/closure', route => {
    if (route.request().method() === 'POST') { closed = true; closure = route.request().postDataJSON(); }
    return route.fulfill({ json: { project_id: projectId, ...closure, status: 'requested', requested_at: timestamp } });
  });
  await page.goto('/#/my-project');
  await page.getByText('申请治疗用款', { exact: true }).click();
  await page.getByLabel('申请金额（元）').fill('10.01');
  await page.getByLabel('收款账户标识').fill('development-account');
  await page.getByLabel('资金用途', { exact: true }).fill('复诊治疗费用');
  await page.locator('.feature-card').getByTestId('upload-project').setInputFiles({ name: 'expense.pdf', mimeType: 'application/pdf', buffer: Buffer.from('%PDF-1.4\n%%EOF') });
  await expect(page.getByLabel('费用证明材料')).toHaveValue('/api/files/' + fileId);
  await page.getByRole('button', { name: '提交用款申请' }).click();
  await expect(page.getByText('等待审核', { exact: true })).toBeVisible();
  expect(disbursement).toMatchObject({ amount: '10.01', purpose: '复诊治疗费用', payout_reference: 'development-account', evidence: ['/api/files/' + fileId], idempotency_key: expect.stringMatching(/^[0-9a-f-]{36}$/) });
  await page.getByLabel('结项原因').fill('治疗阶段结束，剩余款项退还捐助者。');
  await page.getByLabel('剩余资金去向').selectOption('refund');
  await page.getByRole('button', { name: '申请结项并暂停新捐赠' }).click();
  await expect(page.getByRole('heading', { name: '项目正在办理结项' })).toBeVisible();
  await expect(page.getByText('治疗阶段结束，剩余款项退还捐助者。', { exact: true })).toBeVisible();
  await expect(page.getByText('申请治疗用款', { exact: true })).toHaveCount(0);
  expect(closure).toEqual({ reason: '治疗阶段结束，剩余款项退还捐助者。', remaining_plan: 'refund' });
});
