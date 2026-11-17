import { useState } from 'react';
import { Link } from 'react-router-dom';
import { ArrowUpRight, Bell, CheckCheck, RefreshCw } from 'lucide-react';
import { api, errorMessage } from '../lib/api';
import { formatDate } from '../lib/format';
import { Button, Notice, PageHeading } from '../components/ui';
import type { NotificationItem } from './operations-types';
import { OPS_PAGE_SIZE, OpsBadge, OpsPagination, OpsResult, safeInternalLink, useOpsData } from './operations-shared';
import './operations.css';

export default function NotificationsPage() {
  const [offset, setOffset] = useState(0);
  const [busy, setBusy] = useState<string | null>(null);
  const [actionError, setActionError] = useState('');
  const list = useOpsData<NotificationItem[]>('/api/notifications?limit=' + OPS_PAGE_SIZE + '&offset=' + offset, []);
  const unread = useOpsData<{ count: number }>('/api/notifications/unread-count', { count: 0 });
  const refresh = () => { list.reload(); unread.reload(); };
  const markRead = async (id: string | null) => {
    if (busy) return;
    setBusy(id || 'all'); setActionError('');
    try {
      await api(id ? '/api/notifications/' + id + '/read' : '/api/notifications/read-all', { method: 'POST' });
      refresh(); window.dispatchEvent(new Event('crossdo:notifications-updated'));
    } catch (error) { setActionError(errorMessage(error)); }
    finally { setBusy(null); }
  };
  return <div className="page operations-page">
    <PageHeading eyebrow="MESSAGES · 每一步都有回应" title="您的消息" description="审核结果、资料补正与项目进展，都在这里及时告诉您。" action={<OpsBadge tone={unread.data.count ? 'amber' : 'green'}>{unread.loading ? '正在读取…' : unread.error ? '未读数暂不可用' : unread.data.count + ' 条未读消息'}</OpsBadge>} />
    <div className="ops-toolbar"><Button variant="secondary" onClick={refresh} disabled={list.loading}><RefreshCw size={15} />刷新消息</Button><Button variant="secondary" onClick={() => void markRead(null)} busy={busy === 'all'} disabled={!!busy || unread.data.count === 0 || !!unread.error}><CheckCheck size={15} />全部标为已读</Button></div>
    {actionError && <Notice tone="error">{actionError}</Notice>}
    <OpsResult loading={list.loading} error={list.error} empty={!list.data.length} reload={list.reload} emptyTitle="暂时没有新消息" emptyDescription="审核与项目进展会记录在这里。每一份认真，都有回应。">
      <div className="ops-stack">{list.data.map(item => {
        const link = safeInternalLink(item.link);
        return <article key={item.id} className={'ops-card ops-notice-card ' + (item.read_at ? 'is-read' : 'is-unread')}>
          <span className="ops-notice-icon"><Bell size={19} /></span><div>
            <div className="ops-card-top"><h2>{item.title}</h2><OpsBadge tone={item.read_at ? 'neutral' : 'green'}>{item.read_at ? '已读' : '未读'}</OpsBadge></div>
            <p className="ops-copy">{item.body}</p><time>{formatDate(item.created_at)}</time>
            <div className="ops-row-actions">{link && <Link className="ops-inline-link" to={link} onClick={() => { if (!item.read_at) void markRead(item.id); }}>查看相关进展<ArrowUpRight size={15} /></Link>}{!item.read_at && <Button variant="ghost" disabled={!!busy} busy={busy === item.id} onClick={() => void markRead(item.id)}>标为已读</Button>}</div>
          </div>
        </article>;
      })}</div>
    </OpsResult>
    {!list.error && <OpsPagination offset={offset} count={list.data.length} loading={list.loading || !!busy} onChange={setOffset} />}
  </div>;
}
