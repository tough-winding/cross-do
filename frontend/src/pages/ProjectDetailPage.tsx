import { useCallback, useEffect, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { ArrowLeft, ArrowUpRight, CalendarDays, ClipboardCheck, History, ShieldCheck, Sprout, UserRound } from 'lucide-react';
import { api, errorMessage } from '../lib/api';
import { useAuth } from '../lib/auth';
import { formatDate, money, projectTitle } from '../lib/format';
import type { Entry, Project } from '../lib/types';
import type { Funds } from '../features/finance';
import { EmptyState, LoadingState, MaterialLinks, Notice, ReviewHistory, StatusBadge } from '../components/ui';
import { projectProgress } from './ProjectsPage';
import './projects.css';
import ProjectFunding, { ClosureStatus } from '../components/ProjectFunding';

export function entryLinks(entry: Entry): string[] {
  return Array.isArray(entry.attachments) ? entry.attachments : typeof entry.images === 'string' ? entry.images.split(';').filter(Boolean) : [];
}

export function ProjectTimeline({ entries }: { entries: Entry[] }) {
  return <div className="project-panel"><h2 className="project-section-title"><History size={17} /> 康复路上的每一步</h2>
    {entries.length ? <ol className="project-timeline">{entries.slice().reverse().map((entry, index) => <li key={`${entry.time || ''}-${index}`}><time>{formatDate(entry.time)}</time><h3>{index === 0 ? '最近的病情记录' : '病情记录'}</h3><p>{entry.content}</p><MaterialLinks links={entryLinks(entry)} label="相关材料" /></li>)}</ol> : <EmptyState icon={<Sprout size={26} />} title="还没有病情记录" description="申请人提交的病情及后续治疗进展会记录在这里。" />}
  </div>;
}

export default function ProjectDetailPage() {
  const { projectId } = useParams<{ projectId: string }>();
  const { user } = useAuth();
  const [project, setProject] = useState<Project | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [reload, setReload] = useState(0);
  const updateFunds = useCallback((funds: Funds) => { setProject(current => current && current.project_id === funds.project_id ? { ...current, current_donations_received: funds.received, current_used_donations: funds.used } : current); }, []);

  useEffect(() => {
    if (!user || !projectId) { setLoading(false); return; }
    const controller = new AbortController();
    setProject(null); setError(''); setLoading(true);
    api<Project>(`/api/projects/${encodeURIComponent(projectId)}`, { signal: controller.signal })
      .then(setProject)
      .catch((cause: unknown) => { if (!controller.signal.aborted) setError(errorMessage(cause, '暂时无法查看这个项目。')); })
      .finally(() => { if (!controller.signal.aborted) setLoading(false); });
    return () => controller.abort();
  }, [projectId, user?.user_id, reload]);

  if (!user) return <div className="page project-page"><EmptyState title="登录后了解项目详情" description="项目中的病情与审核进展需要登录后查看。" action={<Link className="project-btn primary" to={`/login?next=${encodeURIComponent(`/projects/${projectId || ''}`)}`}>前往登录</Link>} /></div>;
  if (loading) return <LoadingState label="正在了解这份求助…" />;
  if (error || !project) return <div className="page project-page"><Link className="project-breadcrumb" to="/projects"><ArrowLeft size={14} /> 返回项目列表</Link><Notice tone="error">{error || '项目不存在或暂时无法查看。'}</Notice><div><button className="project-btn" onClick={() => setReload(value => value + 1)}>重新加载</button></div></div>;

  const isOwner = project.user_id === user.user_id || user.project_id === project.project_id;
  const isReviewer = project.current_volunteer === user.user_id && [5, 7].includes(user.user_permission);
  return <div className="page project-page project-detail-page">
    <Link className="project-breadcrumb" to="/projects"><ArrowLeft size={14} /> 返回项目列表</Link>
    <div className="project-detail-layout"><div className="project-detail-main">
      <section className="project-panel project-detail-heading"><p className="project-eyebrow">A JOURNEY OF HOPE · 希望接力</p><h1 className="project-detail-title">{projectTitle(project)}</h1><div className="project-detail-meta">{[12, 13].includes(project.project_status) ? <span className={`badge status-${project.project_status}`}><span className="badge-dot" />{project.project_status === 13 ? '已结项' : '结项处理中'}</span> : <StatusBadge status={project.project_status} />}<span><UserRound size={13} /> {project.user_name || '求助者'} · {project.user_age ?? '—'} 岁</span><span><CalendarDays size={13} /> 创建于 {formatDate(project.project_create_time)}</span></div></section>
      {![0, 13].includes(project.project_status) && <Notice>项目尚未公开，仅申请人和当前负责的审核志愿者可查看审核资料。</Notice>}
      <ClosureStatus project={project} />
      <ProjectTimeline entries={project.pathography || []} />
      {[0, 12, 13].includes(project.project_status) && <ProjectFunding project={project} isOwner={isOwner} onFundsLoaded={updateFunds} />}
      {(isOwner || isReviewer) && <div className="project-panel"><h2 className="project-section-title"><ClipboardCheck size={17} /> 审核记录</h2><div style={{ padding: '22px 25px' }}><ReviewHistory records={project.verification_record || []} /></div></div>}
    </div><aside className="project-panel project-funding" aria-label="项目筹款进展"><p className="project-eyebrow">TOGETHER, TOWARD RECOVERY</p><div><p className="project-notice">当前已筹金额</p><div className="project-funding-total">{money(project.current_donations_received)}</div></div>
      <div className="project-progress" role="progressbar" aria-label="筹款进度" aria-valuenow={projectProgress(project)} aria-valuemin={0} aria-valuemax={100}><span style={{ width: `${projectProgress(project)}%` }} /></div>
      <div className="project-funding-row"><span>募款目标</span><strong>{money(project.donation_pool_limit)}</strong></div><div className="project-funding-row"><span>已使用金额</span><strong>{money(project.current_used_donations)}</strong></div>
      <div className="project-funding-divider" /><div className="project-trust-note"><ShieldCheck size={17} /><span>{[0, 13].includes(project.project_status) ? '项目已通过志愿者审核，申请人后续发布的病情进展将在这里持续更新。' : '志愿者将核对项目材料，审核通过后项目才会公开展示。'}</span></div>
      {isOwner ? <Link className="project-btn primary" to="/my-project">管理我的项目 <ArrowUpRight size={15} /></Link> : isReviewer && [3, 7].includes(project.project_status) ? <Link className="project-btn primary" to={`/review/project/${project.project_id}`}>继续项目审核 <ArrowUpRight size={15} /></Link> : <Link className="project-btn primary" to="/donations">查看我的捐赠 <ArrowUpRight size={15} /></Link>}
      <p className="project-notice">最后更新：{formatDate(project.last_update_time)}</p>
    </aside></div>
  </div>;
}
