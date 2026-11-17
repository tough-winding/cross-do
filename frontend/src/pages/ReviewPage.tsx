import { useEffect, useState, type FormEvent } from 'react';
import { Link, useParams } from 'react-router-dom';
import { ArrowLeft, ArrowRight, CheckCircle2, ClipboardCheck, FileHeart, Files, History, ShieldCheck, UserRound } from 'lucide-react';
import { api, ApiError, errorMessage } from '../lib/api';
import { isVolunteer, useAuth } from '../lib/auth';
import { formatDate, money, safeLinks } from '../lib/format';
import type { Application, Entry, Project } from '../lib/types';
import { EmptyState, LoadingState, MaterialLinks, Notice, ReviewHistory, StatusBadge } from '../components/ui';
import './reviews.css';

type Kind = 'application' | 'project';
type Decision = 'approve' | 'correction' | 'unreachable';
const decisions: { value: Decision; label: string; description: string }[] = [
  { value: 'approve', label: '审核通过', description: '已核实身份与相关资料，材料真实完整。' },
  { value: 'correction', label: '需要补正', description: '请在说明中写明需要补充或修改的内容。' },
  { value: 'unreachable', label: '暂时联系不上', description: '暂未能联系申请人，退回后可重新提交。' },
];
const uuidPattern = /^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$/;

function entryLinks(entry: Entry): string[] {
  return safeLinks(entry.attachments?.length ? entry.attachments : entry.images?.split(';') ?? []);
}

export default function ReviewPage() {
  const { kind: kindParam, id = '' } = useParams<{ kind: string; id: string }>();
  const kind: Kind = kindParam === 'project' ? 'project' : 'application';
  const validRoute = (kindParam === 'application' || kindParam === 'project') && uuidPattern.test(id);
  const { user } = useAuth();
  const eligible = isVolunteer(user);
  const [data, setData] = useState<Application | Project | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [saveError, setSaveError] = useState('');
  const [decision, setDecision] = useState<Decision>('approve');
  const [note, setNote] = useState('');
  const [saving, setSaving] = useState(false);
  const [submitted, setSubmitted] = useState(false);
  const [version, setVersion] = useState(0);

  useEffect(() => {
    if (!eligible || !validRoute) { setData(null); setLoading(false); return; }
    const controller = new AbortController();
    setLoading(true); setError(''); setSaveError(''); setData(null); setSubmitted(false); setNote('');
    api<Application | Project>(kind === 'application' ? `/api/applications/${id}` : `/api/projects/${id}`, { signal: controller.signal })
      .then((value) => { if (!controller.signal.aborted) setData(value); })
      .catch((cause: unknown) => {
        if (!controller.signal.aborted) setError(cause instanceof ApiError && cause.status === 403
          ? '这份任务当前不由你负责，或审核已经完成。请返回任务池查看最新状态。'
          : errorMessage(cause, '暂时无法读取审核资料，请稍后重试。'));
      })
      .finally(() => { if (!controller.signal.aborted) setLoading(false); });
    return () => controller.abort();
  }, [eligible, validRoute, kind, id, user?.user_id, version]);

  const application = kind === 'application' && data ? data as Application : null;
  const project = kind === 'project' && data ? data as Project : null;
  const status = application?.user_status ?? project?.project_status ?? 1;
  const assigned = data?.current_volunteer === user?.user_id;
  const canReview = Boolean(eligible && assigned && (status === 3 || status === 7) && !submitted);

  const submit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    if (!canReview || saving || !note.trim()) return;
    if (new TextEncoder().encode(note.trim()).length > (kind === 'application' ? 4000 : 6000)) {
      setSaveError('审核说明较长，请适当精简后再提交。'); return;
    }
    setSaving(true); setSaveError('');
    try {
      const updated = await api<Application | Project>(kind === 'application' ? `/api/applications/${id}/review` : `/api/projects/${id}/review`, {
        method: 'POST', body: { decision, note: note.trim() },
      });
      // An application review releases its reviewer: use this response, never refetch private details.
      setData(updated); setSubmitted(true);
    } catch (cause: unknown) {
      setSaveError(cause instanceof ApiError && (cause.status === 403 || cause.status === 409)
        ? '任务状态已变化，当前无法提交。请返回任务池确认这份任务仍由你处理。'
        : errorMessage(cause, '审核结果未能提交，请保留说明后重试。'));
    } finally { setSaving(false); }
  };

  if (!user || !eligible) return <div className="page review-page"><EmptyState icon={<ShieldCheck size={30} />} title="使用已认证的志愿者账号查看审核任务" description="申请资料仅对当前负责审核的志愿者开放。请登录正确的账号，并从任务池领取任务。" action={<Link className="review-button primary" to={user ? '/account' : '/login?next=%2Ftasks'}>{user ? '查看账户状态' : '去登录'} <ArrowRight size={14} /></Link>} /></div>;
  if (!validRoute) return <div className="page review-page"><EmptyState icon={<ClipboardCheck size={30} />} title="没有找到这份审核任务" description="请从任务池选择需要处理的申请。" action={<Link className="review-button primary" to="/tasks">返回任务池</Link>} /></div>;

  return <div className="page review-page">
    <Link className="review-back" to={`/tasks?type=${kind}`}><ArrowLeft size={14} />返回{kind === 'application' ? '注册资料' : '项目'}任务池</Link>
    {loading ? <LoadingState label="正在读取这份托付的审核资料…" /> : error ? <><Notice tone="error">{error}</Notice><EmptyState icon={<ClipboardCheck size={30} />} title="暂时无法查看这份申请" description="资料的查看权限会随任务归属和审核状态更新。" action={<button className="review-button" type="button" onClick={() => setVersion((value) => value + 1)}>重新加载</button>} /></> : data && <>
      <div className="review-detail-heading"><div><span className="review-eyebrow">{kind === 'application' ? 'IDENTITY & MATERIALS' : 'PROJECT VERIFICATION'}</span><h1>{application?.account.user_name ?? project?.user_name ?? '求助者'}的{kind === 'application' ? '资料审核' : '项目审核'}</h1><p>{kind === 'application' ? '提交时间' : '项目创建时间'} · {formatDate(application?.submitted_at ?? project?.project_create_time)}</p></div><StatusBadge status={status} /></div>
      {submitted && <Notice tone="success"><strong>审核结果已提交</strong> · {decisions.find((item) => item.value === decision)?.label}。谢谢你认真守护每一份信任。</Notice>}
      {!submitted && !canReview && <Notice tone="info">{assigned ? '这份申请已完成当前审核，暂不需要提交新的审核结果。' : '这份申请当前不由你负责，请从任务池领取任务后再进行审核。'}</Notice>}
      <div className="review-detail-grid">
        <div className="review-detail-main">
          <section className="review-detail-card"><div className="review-section-heading"><UserRound size={17} /><h2>申请人资料</h2></div><dl className="review-person">
            <div><dt>申请人昵称</dt><dd>{application?.account.user_name ?? project?.user_name ?? '暂未填写'}</dd></div>
            <div><dt>真实姓名</dt><dd>{application?.account.real_name ?? project?.real_name ?? '暂未填写'}</dd></div>
            <div><dt>年龄</dt><dd>{application?.account.user_age ?? project?.user_age ?? '暂无'} 岁</dd></div>
            {application?.account.phone_number && <div><dt>联系手机</dt><dd>{application.account.phone_number}</dd></div>}
            {project && <div><dt>申请募款目标</dt><dd>{money(project.donation_pool_limit)}</dd></div>}
          </dl></section>
          {application && <section className="review-detail-card"><div className="review-section-heading"><Files size={17} /><h2>已提交的证明材料</h2></div>{application.materials?.length ? <MaterialLinks links={application.materials} label="申请材料" /> : <p className="review-muted">暂未附加证明材料。</p>}<p className="review-muted">请核实材料中的身份、病历与治疗证明，并与申请人确认信息。</p></section>}
          {project && <section className="review-detail-card"><div className="review-section-heading"><FileHeart size={17} /><h2>病情与治疗计划</h2></div>{project.pathography?.length ? <ol className="review-timeline">{project.pathography.map((entry, index) => <li key={`${entry.time ?? 'entry'}-${index}`}><time>{formatDate(entry.time)}</time><p>{entry.content}</p>{entryLinks(entry).length > 0 && <MaterialLinks links={entryLinks(entry)} label={`第 ${index + 1} 次病情材料`} />}</li>)}</ol> : <p className="review-muted">暂未提交病情记录。</p>}</section>}
          <section className="review-detail-card"><div className="review-section-heading"><History size={17} /><h2>审核进展</h2></div><ReviewHistory records={data.verification_record} /></section>
        </div>
        <aside className="review-detail-card review-decision-panel">
          {submitted ? <><div className="review-section-heading"><CheckCircle2 size={19} /><h2>这一程，感谢有你</h2></div><p className="review-muted">{decision === 'approve' ? '审核通过的结果已保存，申请人可以继续接下来的流程。' : decision === 'correction' ? '补正意见已保存，申请人可以根据你的说明完善资料并重新提交。' : '联系情况已记录，申请人可在信息完善后重新提交。'}</p><Link className="review-button primary full" to={`/tasks?type=${kind}`}>返回任务池 <ArrowRight size={14} /></Link></> : canReview ? <form onSubmit={(event) => void submit(event)}><div className="review-section-heading"><ClipboardCheck size={17} /><h2>本次审核结果</h2></div><fieldset disabled={saving}><legend className="sr-only">选择审核结果</legend>{decisions.map((item) => <label className={`review-decision ${decision === item.value ? 'selected' : ''}`} key={item.value}><input type="radio" name="decision" value={item.value} checked={decision === item.value} onChange={() => setDecision(item.value)} /><span><strong>{item.label}</strong><small>{item.description}</small></span></label>)}<label className="review-note-label" htmlFor="review-note">审核说明 <span aria-hidden="true">*</span></label><textarea className="review-note" id="review-note" name="note" required maxLength={2000} placeholder="记录核验情况，或说明需要申请人补充的材料…" value={note} onChange={(event) => setNote(event.target.value)} aria-describedby="review-note-help" /><p className="review-note-count">{note.length} / 2000</p>{saveError && <Notice tone="error">{saveError}</Notice>}<button className="review-button primary full" type="submit" disabled={saving || !note.trim()}>{saving ? '正在保存审核结果…' : '提交审核结果'}{!saving && <ArrowRight size={14} />}</button><p className="review-submit-help" id="review-note-help">你的审核说明会记录在申请进展中，供申请人查看。</p></fieldset></form> : <><div className="review-section-heading"><CheckCircle2 size={19} /><h2>查看审核进展</h2></div><p className="review-muted">当前没有需要你提交的审核结果。可以返回任务池，继续查看其他需要帮助的申请。</p><Link className="review-button primary full" to={`/tasks?type=${kind}`}>返回任务池 <ArrowRight size={14} /></Link></>}
        </aside>
      </div>
    </>}
  </div>;
}
