import { useEffect, useState } from 'react';
import type { FormEvent } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { ArrowUpRight, FileSearch, MessageSquarePlus, RefreshCw, Send } from 'lucide-react';
import { api, errorMessage } from '../lib/api';
import { formatDate } from '../lib/format';
import { Button, Field, MaterialLinks, Notice, PageHeading } from '../components/ui';
import type { CaseReport } from './operations-types';
import { OPS_PAGE_SIZE, OpsBadge, OpsPagination, OpsResult, parseEvidence, recordLabel, useOpsData, UUID_PATTERN } from './operations-shared';
import UploadField from '../components/UploadField';
import './operations.css';

export function CaseDetails({ item }: { item: CaseReport }) {
  return <div className="ops-stack">
    <div className="ops-card-top"><div><h2>{item.kind === 'appeal' ? '申诉详情' : '举报详情'}</h2><p className="ops-id">{item.id}</p></div><OpsBadge tone={item.status === 'open' ? 'amber' : item.status === 'resolved' ? 'green' : 'neutral'}>{recordLabel(item.status)}</OpsBadge></div>
    <div><span className="ops-muted">提交说明</span><p className="ops-copy">{item.reason}</p></div>
    <div className="ops-meta"><span>提交于 {formatDate(item.created_at)}</span>{item.resolved_at && <span>处理于 {formatDate(item.resolved_at)}</span>}</div>
    {item.project_id ? <Link className="ops-inline-link" to={'/projects/' + item.project_id}>查看相关项目<ArrowUpRight size={14} /></Link> : <p className="ops-muted">相关账户：<span className="ops-id">{item.target_user_id}</span></p>}
    {!!item.evidence?.length && <div><h3>证据材料</h3><MaterialLinks links={item.evidence} label="举报申诉证据" /></div>}{item.evidence_private && <p className="ops-muted">相关私密证据由提交人与运营管理员核实，不在此公开。</p>}
    {item.decision_note && <Notice>{item.decision_note}</Notice>}
    {!!item.events?.length && <div><h3>处理记录</h3><ol className="ops-event-list">{item.events.map((event, index) => <li key={event.created_at + '-' + index}><strong>{({ submit: '提交记录', resolved: '处理完成', dismissed: '未予支持', resolve: '运营处理' } as Record<string, string>)[event.action] || '处理进展'}</strong><p>{event.note}</p><time>{formatDate(event.created_at)}</time></li>)}</ol></div>}
  </div>;
}

export default function CasesPage() {
  const [params] = useSearchParams();
  const [offset, setOffset] = useState(0);
  const [targetKind, setTargetKind] = useState<'project' | 'user'>(params.has('target_user_id') ? 'user' : 'project');
  const [target, setTarget] = useState(params.get('project_id') || params.get('target_user_id') || '');
  const [reason, setReason] = useState('');
  const [evidence, setEvidence] = useState('');
  const [parent, setParent] = useState<CaseReport | null>(null);
  const [selected, setSelected] = useState<string | null>(params.get('id'));
  const [showForm, setShowForm] = useState(params.has('project_id') || params.has('target_user_id'));
  const [busy, setBusy] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');
  const list = useOpsData<CaseReport[]>('/api/cases?limit=' + OPS_PAGE_SIZE + '&offset=' + offset, []);
  const detail = useOpsData<CaseReport | null>(selected ? '/api/cases/' + selected : null, null);
  useEffect(() => { if (params.get('id')) setSelected(params.get('id')); }, [params]);
  const beginAppeal = (item: CaseReport) => {
    setParent(item); setReason(''); setEvidence(''); setError(''); setSuccess(''); setShowForm(true);
    document.getElementById('case-compose')?.scrollIntoView({ behavior: 'smooth', block: 'start' });
  };
  const submit = async (event: FormEvent) => {
    event.preventDefault(); if (busy || uploading) return;
    setError(''); setSuccess('');
    try {
      if (!reason.trim()) throw new Error('请说明需要核实的问题或申诉理由。');
      if (!parent && !UUID_PATTERN.test(target.trim())) throw new Error('请填写完整的项目或账户编号。');
      const body = parent ? { kind: 'appeal', parent_id: parent.id, reason: reason.trim(), evidence: parseEvidence(evidence) } : {
        kind: 'report', [targetKind === 'project' ? 'project_id' : 'target_user_id']: target.trim(), reason: reason.trim(), evidence: parseEvidence(evidence),
      };
      setBusy(true);
      const result = await api<CaseReport>('/api/cases', { method: 'POST', body });
      setSuccess(parent ? '申诉已提交，运营人员会结合原记录进行核实。' : '举报已提交，运营人员会核实您提供的情况。');
      setParent(null); setReason(''); setEvidence(''); setShowForm(false); setSelected(result.id); setOffset(0); list.reload();
    } catch (cause) { setError(errorMessage(cause)); }
    finally { setBusy(false); }
  };
  return <div className="page operations-page">
    <PageHeading eyebrow="TRUST & CARE · 认真回应每一个问题" title="举报与申诉" description="发现需要核实的情况，可以提交具体说明；对处理结果有异议，也可以补充材料提出申诉。" action={<Button onClick={() => { setShowForm(value => !value); setParent(null); setError(''); }}><MessageSquarePlus size={16} />{showForm ? '收起表单' : '提交举报'}</Button>} />
    {success && <Notice tone="success">{success}</Notice>}{error && <Notice tone="error">{error}</Notice>}
    {showForm && <section className="ops-card" id="case-compose"><h2>{parent ? '对处理结果提出申诉' : '说明需要核实的情况'}</h2><form className="ops-form" onSubmit={submit}>
      {parent ? <Notice>关联记录：{parent.id}。请说明您对结果的异议，并补充能够帮助核实的材料。</Notice> : <div className="ops-form-grid"><Field label="举报对象" htmlFor="case-target-kind"><select id="case-target-kind" value={targetKind} onChange={e => setTargetKind(e.target.value as 'project' | 'user')} disabled={busy}><option value="project">求助项目</option><option value="user">用户账户</option></select></Field><Field label={targetKind === 'project' ? '项目编号' : '账户编号'} htmlFor="case-target"><input id="case-target" value={target} onChange={e => setTarget(e.target.value)} required maxLength={36} placeholder="完整的项目或账户编号" disabled={busy} /></Field></div>}
      <Field label={parent ? '申诉理由' : '具体情况'} htmlFor="case-reason" hint="请描述可核实的事实、时间及相关经过。"><textarea id="case-reason" value={reason} onChange={e => setReason(e.target.value)} required maxLength={2000} rows={5} disabled={busy} /></Field>
      <UploadField purpose="case" disabled={busy} count={evidence.split(/\r?\n/).filter(line => line.trim()).length} onBusyChange={setUploading} onUploaded={url => setEvidence(value => value.trim() ? value.trim() + '\n' + url : url)} />
      <Field label="补充材料链接（可选）" htmlFor="case-evidence" hint="上传后会自动填入地址，也可补充 HTTP(S) 链接；每行一项，最多 20 项。上传的私密证据仅本人和管理员可查看。"><textarea id="case-evidence" value={evidence} onChange={e => setEvidence(e.target.value)} rows={3} placeholder="https://… 或 /api/files/…" disabled={busy} /></Field>
      <div className="ops-row-actions"><Button type="submit" busy={busy} disabled={uploading}><Send size={15} />{parent ? '提交申诉' : '提交举报'}</Button><Button variant="ghost" type="button" disabled={busy} onClick={() => { setShowForm(false); setParent(null); }}>取消</Button></div>
    </form></section>}
    <div className="ops-toolbar" style={{ marginTop: 22 }}><h2 style={{ fontSize: 17 }}>与我相关的记录</h2><Button variant="secondary" onClick={() => { list.reload(); detail.reload(); }} disabled={list.loading}><RefreshCw size={15} />刷新</Button></div>
    <div className="ops-detail-grid"><div>
      <OpsResult loading={list.loading} error={list.error} empty={!list.data.length} reload={list.reload} emptyTitle="暂时没有相关记录" emptyDescription="您提交或与您相关的举报、申诉及处理进展会显示在这里。">
        <div className="ops-stack">{list.data.map(item => <article className={'ops-card ' + (selected === item.id ? 'ops-selection' : '')} key={item.id}><div className="ops-card-top"><h3><FileSearch size={16} style={{ marginRight: 7, verticalAlign: 'middle' }} />{item.kind === 'appeal' ? '申诉记录' : '举报记录'}</h3><OpsBadge tone={item.status === 'open' ? 'amber' : 'neutral'}>{recordLabel(item.status)}</OpsBadge></div><p className="ops-copy">{item.reason.length > 180 ? item.reason.slice(0, 180) + '…' : item.reason}</p><div className="ops-meta"><span>{formatDate(item.created_at)}</span><span className="ops-id">{item.id.slice(0, 8)}</span></div><div className="ops-row-actions"><Button variant="secondary" onClick={() => setSelected(item.id)}>查看详情</Button>{item.status !== 'open' && <Button variant="ghost" onClick={() => beginAppeal(item)}>提出申诉</Button>}</div></article>)}</div>
      </OpsResult>
      {!list.error && <OpsPagination offset={offset} count={list.data.length} loading={list.loading} onChange={setOffset} />}
    </div><aside className="ops-card">
      {selected ? <OpsResult loading={detail.loading} error={detail.error} empty={!detail.data} reload={detail.reload} emptyTitle="记录暂不可用" emptyDescription="请刷新列表后重新选择记录。">{detail.data && <CaseDetails item={detail.data} />}</OpsResult> : <div className="empty-state"><FileSearch size={31} strokeWidth={1.3} /><h3>查看完整处理进展</h3><p>选择一条记录，查看材料、处理意见及时间线。</p></div>}
    </aside></div>
  </div>;
}
