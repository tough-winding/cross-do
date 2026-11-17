import { useCallback, useEffect, useState } from 'react';
import type { ReactNode } from 'react';
import { ArrowLeft, ArrowRight, RefreshCw } from 'lucide-react';
import { api, errorMessage } from '../lib/api';
import { Button, EmptyState, LoadingState, Notice } from '../components/ui';

export const OPS_PAGE_SIZE = 12;
export const UUID_PATTERN = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
export function useOpsData<T>(path: string | null, initial: T) {
  const [data, setData] = useState<T>(initial);
  const [loading, setLoading] = useState(!!path);
  const [error, setError] = useState('');
  const [version, setVersion] = useState(0);
  const reload = useCallback(() => setVersion(value => value + 1), []);
  useEffect(() => {
    if (!path) { setLoading(false); return; }
    const controller = new AbortController();
    setLoading(true); setError('');
    api<T>(path, { signal: controller.signal }).then(value => {
      if (!controller.signal.aborted) setData(value);
    }).catch(cause => {
      if (!controller.signal.aborted) setError(errorMessage(cause));
    }).finally(() => { if (!controller.signal.aborted) setLoading(false); });
    return () => controller.abort();
  }, [path, version]);
  return { data, setData, loading, error, reload };
}
export function OpsResult({ loading, error, empty, reload, children, emptyTitle = '暂时没有待处理的记录', emptyDescription = '新的记录会显示在这里，可稍后刷新查看。' }: { loading: boolean; error: string; empty: boolean; reload: () => void; children: ReactNode; emptyTitle?: string; emptyDescription?: string }) {
  if (loading) return <LoadingState />;
  if (error) return <div className="ops-stack"><Notice tone="error">{error}</Notice><div><Button variant="secondary" onClick={reload}><RefreshCw size={15} />重新加载</Button></div></div>;
  return empty ? <EmptyState title={emptyTitle} description={emptyDescription} /> : <>{children}</>;
}
export function OpsPagination({ offset, count, loading, onChange }: { offset: number; count: number; loading: boolean; onChange: (offset: number) => void }) {
  return <nav className="ops-pagination" aria-label="记录分页"><Button variant="secondary" disabled={offset === 0 || loading} onClick={() => onChange(Math.max(0, offset - OPS_PAGE_SIZE))}><ArrowLeft size={15} />上一页</Button><span>第 {Math.floor(offset / OPS_PAGE_SIZE) + 1} 页</span><Button variant="secondary" disabled={count < OPS_PAGE_SIZE || loading} onClick={() => onChange(offset + OPS_PAGE_SIZE)}>下一页<ArrowRight size={15} /></Button></nav>;
}
export function OpsBadge({ children, tone = 'neutral' }: { children: ReactNode; tone?: 'neutral' | 'green' | 'amber' | 'red' }) { return <span className={'ops-badge tone-' + tone}>{children}</span>; }
export function recordLabel(status: string): string {
  return ({ open: '等待处理', resolved: '处理完成', dismissed: '未予支持', pending: '等待处理', approved: '审核通过', rejected: '未通过', requested: '等待审核', processing: '处理中', succeeded: '处理成功', failed: '处理失败', paid: '已支付' } as Record<string, string>)[status] || status;
}
export function parseEvidence(value: string, allowLocal = true): string[] {
  const links = value.split(/\r?\n/).map(item => item.trim()).filter(Boolean);
  if (links.length > 20) throw new Error('证据材料最多添加 20 项。');
  for (const link of links) {
    if (allowLocal && /^\/api\/files\/[0-9a-f-]{36}$/i.test(link) && UUID_PATTERN.test(link.slice(11))) continue;
    try { if (link.length <= 2048 && ['http:', 'https:'].includes(new URL(link).protocol)) continue; } catch { /* invalid input */ }
    throw new Error(allowLocal ? '材料请填写有效的 HTTP(S) 文件链接或已上传的文件地址，每行一个。' : '证据材料请填写有效的 HTTP(S) 文件链接，每行一个。');
  }
  return links;
}
export function positiveAmount(value: string): string {
  const amount = value.trim();
  if (!/^(?:0|[1-9][0-9]{0,7})(?:\.[0-9]{1,2})?$/.test(amount) || cents(amount) <= 0n) throw new Error('请输入大于零、最多两位小数的金额。');
  return decimal(cents(amount));
}
export function cents(value: string | undefined): bigint {
  const match = /^(\d+)(?:\.(\d{1,2}))?$/.exec(value || '0');
  if (!match) return 0n;
  return BigInt(match[1]) * 100n + BigInt((match[2] || '').padEnd(2, '0'));
}
export function decimal(value: bigint): string { return String(value / 100n) + '.' + String(value % 100n).padStart(2, '0'); }
export function safeInternalLink(value?: string | null): string | null {
  return value && value.startsWith('/') && !value.startsWith('//') && !/[\\\r\n]/.test(value) && !value.startsWith('/api/') ? value : null;
}
