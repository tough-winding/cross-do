import { expect, test, type Page } from '@playwright/test';

const userId = '10000000-0000-4000-8000-000000000001';
const projectId = '20000000-0000-4000-8000-000000000001';
const orderId = '30000000-0000-4000-8000-000000000001';
const caseId = '40000000-0000-4000-8000-000000000001';
const fileId = '50000000-0000-4000-8000-000000000001';
const createdAt = '2026-11-06 09:00:00';

// These tests isolate browser interactions from backend integration coverage.
// All API requests are intercepted; no test account or real money is involved.
async function session(page: Page, role = 1) {
  await page.addInitScript(() => sessionStorage.setItem('crossdo.session', JSON.stringify({ token: 'test-session-token', expiresAt: Date.now() + 3_600_000 })));
  await page.route('**/api/**', async route => {
    const path = new URL(route.request().url()).pathname;
    if (path === '/api/me') return route.fulfill({ json: { user_id: userId, user_name: '交互测试账户', user_permission: role, user_status: 0, real_name_authentication: 1, phone_number: '19000000000', account_state: 'active', phone_verified: 1 } });
    if (path === '/api/notifications/unread-count') return route.fulfill({ json: { count: 0 } });
    return route.fulfill({ status: 404, json: { message: '测试未配置接口' } });
  });
}

test('普通账户访问运营页面时不请求管理员数据', async ({ page }) => {
  await session(page, 2);
  const adminRequests: string[] = [];
  page.on('request', request => { if (new URL(request.url()).pathname.startsWith('/api/admin/')) adminRequests.push(request.url()); });
  await page.goto('/#/admin');
  await expect(page.getByRole('heading', { name: '这里需要运营管理员身份' })).toBeVisible();
  expect(adminRequests).toEqual([]);
});

test('退款保留精确金额与失败后刷新重试的幂等编号', async ({ page }) => {
  await session(page);
  await page.setViewportSize({ width: 390, height: 844 });
  const requests: { amount: string; reason: string; idempotency_key: string }[] = [];
  const refund = { id: fileId, order_id: orderId, project_id: projectId, amount: '1.01', reason: '用户申请退回', status: 'pending', mode: 'development', created_at: createdAt };
  await page.route('**/api/payments/settings', route => route.fulfill({ json: { mode: 'development' } }));
  await page.route('**/api/admin/finance/orders?*', route => route.fulfill({ json: [{ id: orderId, project_id: projectId, donor_id: userId, amount: '100.30', refunded_amount: '20.10', refund_reserved: requests.length > 1 ? '1.11' : '0.10', currency: 'CNY', status: 'paid', mode: 'development', checkout_url: null, created_at: createdAt, refunds: requests.length > 1 ? [refund] : [] }] }));
  await page.route('**/api/donations/' + orderId + '/refunds', route => {
    requests.push(route.request().postDataJSON());
    return requests.length === 1 ? route.fulfill({ status: 500, json: { message: 'Retry test' } }) : route.fulfill({ status: 201, json: refund });
  });
  await page.goto('/#/admin?tab=finance');
  await page.getByRole('button', { name: '发起退款', exact: true }).click();
  await expect(page.getByLabel('退款金额（元）')).toHaveValue('80.10');
  await page.getByLabel('退款金额（元）').fill('1.01');
  await page.getByLabel('退款原因').fill('用户申请退回');
  await page.getByRole('button', { name: '提交退款申请' }).click();
  await expect(page.getByText('服务暂时不可用，请稍后重试。')).toBeVisible();
  await expect(page.getByLabel('退款金额（元）')).toHaveValue('1.01');
  await page.reload();
  await page.getByRole('button', { name: '发起退款', exact: true }).click();
  await page.getByLabel('退款金额（元）').fill('1.01');
  await page.getByLabel('退款原因').fill('用户申请退回');
  await page.getByRole('button', { name: '提交退款申请' }).click();
  await expect(page.getByRole('heading', { name: '退款记录' })).toBeVisible();
  expect(requests).toHaveLength(2);
  expect(requests[0].idempotency_key).toMatch(/^[0-9a-f-]{36}$/);
  expect(requests[1]).toEqual(requests[0]);
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
});

test('移动端举报上传私密证据并提交文件引用', async ({ page }) => {
  await session(page, 2);
  await page.setViewportSize({ width: 390, height: 844 });
  let submitted: Record<string, unknown> | null = null;
  let uploadBody = '';
  const report = { id: caseId, reporter_id: userId, target_user_id: null, project_id: projectId, parent_id: null, kind: 'report', reason: '请核实项目材料中的日期与治疗记录。', evidence: ['/api/files/' + fileId], status: 'open', decision_note: null, created_at: createdAt, updated_at: createdAt, resolved_at: null };
  await page.route('**/api/cases?*', route => route.fulfill({ json: submitted ? [report] : [] }));
  await page.route('**/api/cases/' + caseId, route => route.fulfill({ json: { ...report, events: [] } }));
  await page.route('**/api/files', route => {
    uploadBody = route.request().postData() || '';
    expect(route.request().headers().authorization).toBe('Bearer test-session-token');
    return route.fulfill({ status: 201, json: { id: fileId, name: 'evidence.pdf', size: 15, mime_type: 'application/pdf', url: '/api/files/' + fileId } });
  });
  await page.route('**/api/cases', route => { submitted = route.request().postDataJSON(); return route.fulfill({ status: 201, json: report }); });
  await page.goto('/#/cases?project_id=' + projectId);
  await page.getByLabel('具体情况').fill(report.reason);
  await page.getByTestId('upload-case').setInputFiles({ name: 'evidence.pdf', mimeType: 'application/pdf', buffer: Buffer.from('%PDF-1.4\n%%EOF') });
  await expect(page.getByLabel('补充材料链接（可选）')).toHaveValue('/api/files/' + fileId);
  expect(uploadBody).toMatch(/name="purpose"\r\n\r\ncase/);
  await page.locator('#case-compose').getByRole('button', { name: '提交举报', exact: true }).click();
  await expect(page.getByText('举报已提交，运营人员会核实您提供的情况。')).toBeVisible();
  await expect(page.getByRole('heading', { name: '举报详情' })).toBeVisible();
  expect(submitted).toEqual({ kind: 'report', project_id: projectId, reason: report.reason, evidence: ['/api/files/' + fileId] });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
});

test('通知已读同步计数并拒绝外部跳转链接', async ({ page }) => {
  await session(page, 2);
  let read = false;
  await page.route('**/api/notifications/unread-count', route => route.fulfill({ json: { count: read ? 0 : 1 } }));
  await page.route('**/api/notifications?*', route => route.fulfill({ json: [{ id: caseId, type: 'case', title: '处理结果已更新', body: '您提交的记录已有处理意见。', link: '//external.example/unsafe', created_at: createdAt, read_at: read ? createdAt : null }] }));
  await page.route('**/api/notifications/' + caseId + '/read', route => { read = true; return route.fulfill({ status: 204 }); });
  await page.goto('/#/notifications');
  await expect(page.getByText('1 条未读消息')).toBeVisible();
  await expect(page.getByRole('link', { name: '查看相关进展' })).toHaveCount(0);
  await page.getByRole('button', { name: '标为已读', exact: true }).click();
  await expect(page.getByText('0 条未读消息')).toBeVisible();
  await expect(page.getByRole('button', { name: '全部标为已读' })).toBeDisabled();
});

test('网关恢复复用原退款和发款申请，未启用服务时禁用重试', async ({ page }) => {
  await session(page);
  let mode = 'disabled';
  let refundRetries = 0;
  let payoutReview: unknown = null;
  const refund = { id: fileId, order_id: orderId, project_id: projectId, amount: '10.00', reason: '原退款申请', status: 'pending', mode: 'gateway', created_at: createdAt };
  const payout = { id: caseId, project_id: projectId, requester_id: userId, amount: '5.00', purpose: '治疗支出', evidence: [], payout_reference: 'TEST-REFERENCE', status: 'approved', mode: 'gateway', review_note: '用途已核实', created_at: createdAt };
  await page.route('**/api/payments/settings', route => route.fulfill({ json: { mode } }));
  await page.route('**/api/admin/finance/orders?*', route => route.fulfill({ json: [{ id: orderId, project_id: projectId, donor_id: userId, amount: '10.00', refunded_amount: '0.00', refund_reserved: '10.00', status: 'paid', mode: 'gateway', created_at: createdAt, refunds: [refund] }] }));
  await page.route('**/api/refunds/' + fileId + '/retry', route => { refundRetries++; return route.fulfill({ json: refund }); });
  await page.route('**/api/admin/disbursements?*', route => route.fulfill({ json: [payout] }));
  await page.route('**/api/admin/disbursements/' + caseId + '/review', route => { payoutReview = route.request().postDataJSON(); return route.fulfill({ json: payout }); });
  await page.goto('/#/admin?tab=finance');
  await expect(page.getByRole('button', { name: '重试原退款申请' })).toBeDisabled();
  mode = 'gateway';
  await page.reload();
  await page.getByRole('button', { name: '重试原退款申请' }).click();
  await expect.poll(() => refundRetries).toBe(1);
  await page.getByRole('tab', { name: '资金使用审核', exact: true }).click();
  await page.getByRole('button', { name: '重试原发款申请' }).click();
  await expect.poll(() => payoutReview).toEqual({ decision: 'approve', note: payout.review_note });
});
