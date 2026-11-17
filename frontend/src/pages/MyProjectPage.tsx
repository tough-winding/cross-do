import { useEffect, useState, type FormEvent } from 'react';
import { Link } from 'react-router-dom';
import { ArrowRight, Check, ClipboardCheck, FileHeart, History, Link2, Plus, Send, ShieldCheck, Sprout } from 'lucide-react';
import { api, errorMessage } from '../lib/api';
import { useAuth } from '../lib/auth';
import { formatDate, projectTitle } from '../lib/format';
import type { Entry, Project } from '../lib/types';
import { EmptyState, LoadingState, Notice, PageHeading, ReviewHistory, StatusBadge } from '../components/ui';
import { entryLinks, ProjectTimeline } from './ProjectDetailPage';
import './projects.css';
import UploadField from '../components/UploadField';
import { references as attachmentReferences } from '../features/files';
import { OwnerFunding } from '../components/ProjectFunding';


function validContent(value: string): string {
  const content = value.trim();
  if (!content) throw new Error('请填写病情说明和治疗计划。');
  if (content.length > 10000 || new TextEncoder().encode(content).length > 12000) throw new Error('病情说明较长，请精简到约 4000 个汉字以内。');
  return content;
}

export default function MyProjectPage() {
  const { user, refreshUser } = useAuth();
  const [project, setProject] = useState<Project | null>(null);
  const [loading, setLoading] = useState(Boolean(user?.project_id));
  const [busy, setBusy] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');
  const [amount, setAmount] = useState('');
  const [content, setContent] = useState('');
  const [references, setReferences] = useState('');
  const [updateContent, setUpdateContent] = useState('');
  const [updateReferences, setUpdateReferences] = useState('');
  const [reload, setReload] = useState(0);
  const [profileLoading, setProfileLoading] = useState(true);
  const [profileError, setProfileError] = useState('');
  const [profileReload, setProfileReload] = useState(0);

  const populate = (value: Project) => {
    setProject(value);
    setAmount(value.donation_pool_limit && value.donation_pool_limit !== '0.00' ? value.donation_pool_limit : '');
    const last = value.pathography?.at(-1);
    setContent(last?.content || '');
    setReferences(last ? entryLinks(last).join('\n') : '');
  };

  useEffect(() => {
    let active = true;
    setProfileLoading(true);
    refreshUser()
      .then(() => { if (active) setProfileError(''); })
      .catch((cause: unknown) => { if (active) setProfileError(errorMessage(cause, '暂时无法刷新账号资料，请重试。')); })
      .finally(() => { if (active) setProfileLoading(false); });
    return () => { active = false; };
  }, [refreshUser, profileReload]);

  useEffect(() => {
    if (!user?.project_id) { setProject(null); setLoading(false); return; }
    const controller = new AbortController();
    setLoading(true); setError('');
    api<Project>(`/api/projects/${encodeURIComponent(user.project_id)}`, { signal: controller.signal })
      .then(populate)
      .catch((cause: unknown) => { if (!controller.signal.aborted) setError(errorMessage(cause, '项目加载失败，请稍后重试。')); })
      .finally(() => { if (!controller.signal.aborted) setLoading(false); });
    return () => controller.abort();
  }, [user?.user_id, user?.project_id, reload]);

  async function createProject() {
    setBusy(true); setError(''); setSuccess('');
    try {
      const value = await api<Project>('/api/projects', { method: 'POST' });
      populate(value);
      setSuccess('项目草稿已创建。请填写病情与募款目标，提交给志愿者审核。');
      try {
        await refreshUser();
        setProfileError('');
      } catch {
        setProfileError('项目已创建成功，但账号资料暂未刷新。您可以继续填写当前草稿，或刷新账号资料同步绑定状态。');
      }
    } catch (cause) { setError(errorMessage(cause, '暂时无法创建项目，请稍后重试。')); }
    finally { setBusy(false); }
  }

  async function submitProject(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!project || busy) return;
    setBusy(true); setError(''); setSuccess('');
    try {
      const target = amount.trim();
      if (!/^(?:0|[1-9][0-9]{0,7})(?:\.[0-9]{1,2})?$/.test(target) || Number(target) <= 0) throw new Error('请填写有效的募款目标，金额大于 0，最多两位小数。');
      const entry: Entry = { content: validContent(content), attachments: attachmentReferences(references) };
      const earlier = (project.pathography || []).slice(0, -1).map(item => ({ content: item.content, attachments: entryLinks(item) }));
      const value = await api<Project>(`/api/projects/${project.project_id}/submit`, { method: 'POST', body: { donation_pool_limit: target, pathography: [...earlier, entry] } });
      setProject(value);
      setSuccess('项目材料已提交。请耐心等待志愿者接单，审核进展会记录在这里。');
    } catch (cause) { setError(errorMessage(cause, '项目提交失败，请稍后重试。')); }
    finally { setBusy(false); }
  }

  async function appendProgress(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!project || busy) return;
    setBusy(true); setError(''); setSuccess('');
    try {
      const value = await api<Project>(`/api/projects/${project.project_id}/pathography`, { method: 'POST', body: { entry: { content: validContent(updateContent), attachments: attachmentReferences(updateReferences) } } });
      setProject(value); setUpdateContent(''); setUpdateReferences('');
      setSuccess('新的病情进展已发布，关心您的人现在可以了解这一步变化。');
    } catch (cause) { setError(errorMessage(cause, '病情进展发布失败，请稍后重试。')); }
    finally { setBusy(false); }
  }

  if (profileLoading) return <LoadingState label="正在确认最新的账号与项目状态…" />;
  if (profileError && !project) return <div className="page project-page"><PageHeading eyebrow="MY PROJECT · 我的项目" title="确认最新进展，再继续下一步。" /><Notice tone="error">{profileError}</Notice><div><button className="project-btn" onClick={() => setProfileReload(value => value + 1)}>刷新账号资料</button></div></div>;
  if (!user) return <div className="page project-page"><EmptyState title="登录后管理自己的项目" description="保存病情、提交申请，和志愿者一起推进审核。" action={<Link className="project-btn primary" to="/login?next=%2Fmy-project">前往登录</Link>} /></div>;
  if (user.user_permission !== 3) return <div className="page project-page"><PageHeading eyebrow="MY PROJECT" title="陪伴每一段康复旅程" /><div className="project-panel"><EmptyState icon={<FileHeart size={28} />} title="项目申请由患者账号发起" description="您可以前往公开项目，了解通过审核的求助与治疗进展。" action={<Link className="project-btn primary" to="/projects">了解求助项目 <ArrowRight size={15} /></Link>} /></div></div>;
  if (user.user_status !== 0 || user.real_name_authentication !== 1) return <div className="page project-page"><PageHeading eyebrow="MY PROJECT · 我的项目" title="让希望，从一份真实的资料开始。" /><div className="project-panel"><EmptyState icon={<ShieldCheck size={28} />} title="请先完成患者资料审核" description="身份与医疗材料审核通过后，就可以创建项目、填写募款目标，并提交项目审核。" action={<Link className="project-btn primary" to="/application">查看我的资料申请 <ArrowRight size={15} /></Link>} /></div></div>;
  if (loading) return <LoadingState label="正在加载您的项目…" />;

  const editable = project && [1, 4, 5].includes(project.project_status);
  const approved = project?.project_status === 0;
  const publiclyReviewed = project && [0, 12, 13].includes(project.project_status);
  const correction = project && [4, 5].includes(project.project_status);
  const lastReview = project?.verification_record?.slice().reverse().find(record => ['correction', 'unreachable'].includes(record.decision));
  return <div className="page project-page my-project-page">
    <PageHeading eyebrow="MY PROJECT · 我的项目" title={project ? projectTitle(project) : '为康复的下一步，留一束光。'} description="认真记录每一步治疗，和志愿者一起，让真实的求助被看见。" action={project ? [12, 13].includes(project.project_status) ? <span className={`badge status-${project.project_status}`}><span className="badge-dot" />{project.project_status === 13 ? '已结项' : '结项处理中'}</span> : <StatusBadge status={project.project_status} /> : undefined} />
    <div className="project-step-row" aria-label="项目申请步骤"><div className="project-step done"><span><Check size={14} /></span><div><strong>01 · 患者资料认证</strong><p>已完成身份与材料审核</p></div></div><div className={`project-step ${!project || editable ? 'current' : 'done'}`}><span>{project && !editable ? <Check size={14} /> : '02'}</span><div><strong>02 · 提交项目资料</strong><p>说明病情、计划和募款目标</p></div></div><div className={`project-step ${publiclyReviewed ? 'done' : project && !editable ? 'current' : ''}`}><span>{publiclyReviewed ? <Check size={14} /> : '03'}</span><div><strong>03 · 志愿者项目审核</strong><p>{publiclyReviewed ? '项目已通过审核并公开' : '核实材料后公开项目'}</p></div></div></div>
    {error && <Notice tone="error">{error}</Notice>}{success && <Notice tone="success">{success}</Notice>}
    {profileError && <Notice>{profileError} <button className="project-btn" type="button" onClick={() => setProfileReload(value => value + 1)}>刷新账号资料</button></Notice>}
    {!project ? user.project_id && error ? <div><button className="project-btn" onClick={() => setReload(value => value + 1)}>重新加载项目</button></div> : <div className="project-panel"><EmptyState icon={<Sprout size={30} />} title="准备好了，就让希望启程" description="您的患者资料已经审核通过。创建项目草稿后，填写病情说明和治疗目标，即可提交志愿者审核。" action={<button className="project-btn primary" data-testid="project-create" onClick={createProject} disabled={busy}><Plus size={16} />{busy ? '正在创建…' : '创建我的项目'}</button>} /></div> : <>
      {correction && <Notice>{project.project_status === 4 ? '志愿者暂时未能联系到您，请确认联系方式，并重新提交项目资料。' : '请根据志愿者的意见补充资料，再次提交审核。'}{lastReview?.note ? <span> 审核意见：{lastReview.note}</span> : null}</Notice>}
      {editable ? <section className="project-panel"><h2 className="project-section-title"><FileHeart size={17} /> {correction ? '补充项目资料' : '填写项目资料'}</h2><form className="project-form" onSubmit={submitProject}>
        <div className="project-form-grid"><div className="project-field"><label htmlFor="project-target">募款目标（元）</label><input id="project-target" className="project-input" type="text" inputMode="decimal" required maxLength={11} value={amount} onChange={event => setAmount(event.target.value)} placeholder="例如 10000.00" disabled={busy} /><small>请按照医生的治疗计划填写，精确到分。</small></div><div className="project-message"><ShieldCheck size={17} /><span>项目公开前，志愿者会再次核对病情与费用依据。请确保所填写的信息真实、完整。</span></div></div>
        <div className="project-field"><label htmlFor="project-content">病情与治疗计划</label><textarea id="project-content" className="project-input" rows={7} required maxLength={10000} value={content} onChange={event => setContent(event.target.value)} placeholder="请介绍目前的病情、治疗方案和需要帮助的地方。" disabled={busy} /><small>尽量避免填写身份证号、银行卡号等不需要公开的个人信息。</small></div>
        <div className="project-field"><label htmlFor="project-references">材料链接（每行一个）</label><textarea id="project-references" className="project-input" rows={3} value={references} onChange={event => setReferences(event.target.value)} placeholder="https://…/治疗计划.pdf\nhttps://…/费用说明.jpg" disabled={busy} /><small>最多 20 份材料，可上传文件或填写已有文件的 HTTP(S) 链接。</small></div>
        <UploadField purpose="project" projectId={project.project_id} count={references.split("\n").filter(Boolean).length} disabled={busy} onBusyChange={setUploading} onUploaded={url => setReferences(current => [current, url].filter(Boolean).join("\n"))} /><div className="project-form-footer"><p className="project-notice"><Link2 size={12} style={{ verticalAlign: '-2px', marginRight: 5 }} />审核意见由志愿者填写，提交后可在这里查看进展。</p><button className="project-btn primary" type="submit" data-testid="project-submit" disabled={busy || uploading}><Send size={14} />{busy ? '正在提交…' : correction ? '重新提交审核' : '提交项目审核'}</button></div>
      </form></section> : approved ? <section className="project-panel"><h2 className="project-section-title"><Plus size={17} /> 记录新的康复进展</h2><form className="project-form" onSubmit={appendProgress}>
        <div className="project-field"><label htmlFor="project-update-content">新的病情进展</label><textarea id="project-update-content" className="project-input" rows={5} required maxLength={10000} value={updateContent} onChange={event => setUpdateContent(event.target.value)} placeholder="今天的治疗有什么新变化？和关心您的人分享这一步。" disabled={busy} /></div><div className="project-field"><label htmlFor="project-update-references">进展附件（每行一个）</label><textarea id="project-update-references" className="project-input" rows={2} value={updateReferences} onChange={event => setUpdateReferences(event.target.value)} placeholder="https://…/复查报告.pdf" disabled={busy} /><small>可上传复查文件或填写已有文件链接，也可以留空。</small></div>
        <UploadField purpose="project" projectId={project.project_id} count={updateReferences.split("\n").filter(Boolean).length} disabled={busy} onBusyChange={setUploading} onUploaded={url => setUpdateReferences(current => [current, url].filter(Boolean).join("\n"))} /><div className="project-form-footer"><Link className="project-text-link" to={`/projects/${project.project_id}`}>查看公开项目 <ArrowRight size={14} /></Link><button className="project-btn primary" type="submit" data-testid="pathography-submit" disabled={busy || uploading}><Send size={14} />{busy ? '正在发布…' : '发布病情进展'}</button></div>
      </form></section> : <div className="project-panel"><EmptyState icon={<ClipboardCheck size={29} />} title={[2, 6].includes(project.project_status) ? '项目已提交，等待志愿者接单' : [3, 7].includes(project.project_status) ? '志愿者正在认真核对您的材料' : project.project_status === 13 ? '项目已完成结项' : project.project_status === 12 ? '结项申请正在处理' : '项目当前无法编辑'} description={[2, 3, 6, 7].includes(project.project_status) ? '审核期间资料暂不可修改。需要补充材料时，审核意见会显示在这里。' : project.project_status === 13 ? '感谢每一份帮助。结项原因、病情记录和资金去向会继续保留。' : project.project_status === 12 ? '新捐赠已暂停，运营人员会核对剩余资金并完成结项。' : '请查看下面的审核记录，了解项目当前进展。'} action={<button className="project-btn" onClick={() => setReload(value => value + 1)}>刷新审核进展</button>} /></div>}
      {[0, 12, 13].includes(project.project_status) && <OwnerFunding project={project} onChanged={() => setReload(value => value + 1)} />}
      {!!project.pathography?.length && !editable && <ProjectTimeline entries={project.pathography} />}
      <section className="project-panel"><h2 className="project-section-title"><History size={17} /> 项目审核记录 <span style={{ marginLeft: 'auto', fontSize: 10, fontWeight: 400, color: '#8c9685' }}>最后更新 {formatDate(project.last_update_time)}</span></h2><div style={{ padding: '22px 25px' }}><ReviewHistory records={project.verification_record || []} /></div></section>
    </>}
  </div>;
}
