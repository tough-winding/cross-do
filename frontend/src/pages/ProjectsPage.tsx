import { useEffect, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { ArrowLeft, ArrowRight, ArrowUpRight, CheckCheck, Clock3, HeartHandshake, Search, ShieldCheck, Sprout, Stethoscope } from 'lucide-react';
import { api, errorMessage } from '../lib/api';
import { useAuth } from '../lib/auth';
import { formatDate, money, projectTitle } from '../lib/format';
import type { Project, ProjectSummary } from '../lib/types';
import { EmptyState, Notice, PageHeading } from '../components/ui';
import './projects.css';

const PAGE_SIZE = 6;

/** Progress is calculated in integer cents; no floating point financial arithmetic. */
export function projectProgress(project: Pick<Project, 'donation_pool_limit' | 'current_donations_received'>): number {
  const cents = (value: string | null | undefined): bigint => {
    const match = /^(\d+)(?:\.(\d{1,2}))?$/.exec(value || '0');
    return match ? BigInt(match[1]) * 100n + BigInt((match[2] || '').padEnd(2, '0')) : 0n;
  };
  const target = cents(project.donation_pool_limit);
  if (target <= 0n) return 0;
  const percentage = cents(project.current_donations_received) * 100n / target;
  return Number(percentage > 100n ? 100n : percentage);
}

export default function ProjectsPage() {
  const { user } = useAuth();
  const [projects, setProjects] = useState<ProjectSummary[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [query, setQuery] = useState('');
  const [page, setPage] = useState(0);
  const [reload, setReload] = useState(0);

  useEffect(() => {
    if (!user) { setProjects([]); return; }
    const controller = new AbortController();
    setLoading(true);
    setError('');
    api<ProjectSummary[]>(`/api/projects?summary=true&limit=${PAGE_SIZE}&offset=${page * PAGE_SIZE}`, { signal: controller.signal })
      .then(setProjects)
      .catch((cause: unknown) => { if (!controller.signal.aborted) setError(errorMessage(cause, '项目加载失败，请稍后重试。')); })
      .finally(() => { if (!controller.signal.aborted) setLoading(false); });
    return () => controller.abort();
  }, [user?.user_id, page, reload]);

  const visible = useMemo(() => {
    const value = query.trim().toLocaleLowerCase();
    if (!value) return projects;
    return projects.filter(project => [project.user_name, project.summary]
      .join(' ').toLocaleLowerCase().includes(value));
  }, [projects, query]);

  return <div className="page project-page projects-page">
    <PageHeading eyebrow="PROJECTS · 善意接力" title="每一份求助，都值得被认真看见。" description="走近已通过审核的求助项目，了解每一段康复路上的真实进展。" action={<div className="project-protection"><ShieldCheck size={15} /> 志愿者审核 · 进展可追溯</div>} />
    {!user ? <div className="project-panel"><EmptyState icon={<HeartHandshake size={30} />} title="让我们从一份了解开始" description="登录后，查看已通过志愿者审核的项目与最新病情进展。每一份理解，都是帮助的开始。" action={<Link className="project-btn primary" to="/login?next=%2Fprojects">登录查看项目 <ArrowRight size={15} /></Link>} /></div> : <>
      <div className="project-list-controls">
        <div className="project-filter-tabs"><span className="project-filter-label"><CheckCheck size={16} /> 已通过审核 <span className="project-counter">当前页 {loading ? '…' : visible.length} 项</span></span></div>
        <div className="project-search"><Search size={16} /><label className="sr-only" htmlFor="project-search">搜索当前页项目</label><input id="project-search" value={query} onChange={event => setQuery(event.target.value)} placeholder="搜索当前页的昵称或摘要" /></div>
      </div>
      {error ? <><Notice tone="error">{error}</Notice><div><button className="project-btn" onClick={() => setReload(value => value + 1)}>重新加载</button></div></> : loading ? <div className="project-grid" role="status" aria-label="正在加载项目">{Array.from({ length: 6 }, (_, index) => <div className="project-skeleton" key={index} aria-hidden="true" />)}</div> : visible.length ? <div className="project-grid">
        {visible.map((project, index) => {
          const illustration = [Sprout, Stethoscope, HeartHandshake][index % 3];
          const Illustration = illustration;
          const description = project.summary || '申请人的项目材料已经通过志愿者审核，后续病情进展将在项目中持续记录。';
          return <article className="project-card" key={project.project_id}>
            <div className={`project-card-art art-${index % 3}`} aria-hidden="true"><span className="project-verified"><ShieldCheck size={11} /> 审核通过</span><Illustration className="project-art-symbol" size={56} strokeWidth={1} /></div>
            <div className="project-card-body"><div className="project-card-heading"><span className="project-avatar" aria-hidden="true">{(project.user_name || '渡').slice(0, 1)}</span><p className="project-owner">{project.user_name || '求助者'} · {project.user_age ?? '—'} 岁</p></div>
              <h2>{projectTitle(project)}</h2><p className="project-description">{description}</p>
              <div className="project-progress" role="progressbar" aria-label="已筹金额占募款目标比例" aria-valuenow={projectProgress(project)} aria-valuemin={0} aria-valuemax={100}><span style={{ width: `${projectProgress(project)}%` }} /></div>
              <div className="project-money-line"><strong>{money(project.current_donations_received)}</strong><span>目标 {money(project.donation_pool_limit)}</span></div>
              <div className="project-card-footer"><time dateTime={project.last_update_time || undefined}><Clock3 size={11} /> {formatDate(project.last_update_time)}</time><Link className="project-text-link" to={`/projects/${project.project_id}`}>了解项目 <ArrowUpRight size={14} /></Link></div>
            </div>
          </article>;
        })}
      </div> : <div className="project-panel"><EmptyState icon={<Sprout size={28} />} title={query ? '当前页还没有匹配的项目' : '善意，正在等待下一次启程'} description={query ? '试试其他关键词，或清空搜索查看当前页全部项目。' : '目前这一页没有已通过审核的项目。新的申请通过志愿者审核后，会出现在这里。'} action={query ? <button className="project-btn" onClick={() => setQuery('')}>清空搜索</button> : page ? <button className="project-btn" onClick={() => setPage(value => value - 1)}>返回上一页</button> : undefined} /></div>}
      {!error && <div className="project-pagination"><span>第 {page + 1} 页 · 每页最多 {PAGE_SIZE} 个项目</span><div className="project-pagination-actions"><button className="project-btn" disabled={!page || loading} onClick={() => { setPage(value => value - 1); setQuery(''); }}><ArrowLeft size={13} /> 上一页</button><button className="project-btn" disabled={projects.length < PAGE_SIZE || loading} onClick={() => { setPage(value => value + 1); setQuery(''); }}>下一页 <ArrowRight size={13} /></button></div></div>}
    </>}
  </div>;
}
