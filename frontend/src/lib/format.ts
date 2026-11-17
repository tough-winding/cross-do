import type { Project } from './types';

export function money(value?: string | number | null): string {
  const input = String(value ?? '0');
  const match = /^(-?)(\d+)(?:\.(\d{1,2}))?$/.exec(input);
  if (!match) return '¥0.00';
  const [, sign, integer, fraction = ''] = match;
  return `${sign}¥${integer.replace(/\B(?=(\d{3})+(?!\d))/g, ',')}.${fraction.padEnd(2, '0')}`;
}
export function formatDate(value?: string | null): string {
  if (!value) return '暂无';
  const date = new Date(value.includes('T') ? value : value.replace(' ', 'T') + 'Z');
  if (Number.isNaN(date.getTime())) return '暂无';
  return new Intl.DateTimeFormat('zh-CN', { timeZone: 'Asia/Shanghai', year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hour12: false }).format(date);
}
export function statusLabel(status: number): string {
  return ({ 0: '审核通过', 1: '待完善资料', 2: '等待志愿者接单', 3: '志愿者审核中', 4: '暂未联系成功', 5: '需要补充资料', 6: '重新提交待审核', 7: '补充资料审核中', 8: '已被举报', 9: '暂未启用', 10: '已冻结', 11: '申诉处理中', 12: '注销处理中', 13: '已注销' } as Record<number, string>)[status] || '状态待确认';
}
export function roleLabel(role: number): string {
  return ({ 1: '管理员', 2: '爱心捐助者', 3: '求助患者', 5: '志愿者', 6: '长期捐助者', 7: '资深志愿者' } as Record<number, string>)[role] || '用户';
}
export function projectTitle(project: Pick<Project, 'user_name'>): string { return `为${project.user_name || '求助者'}，接力一份希望`; }
export function safeLinks(links: unknown): string[] {
  if (!Array.isArray(links)) return [];
  return links.filter((link): link is string => {
    if (typeof link !== 'string') return false;
    if (/^\/api\/files\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(link)) return true;
    try { return ['http:', 'https:'].includes(new URL(link).protocol); } catch { return false; }
  });
}
