import { useEffect, useState } from 'react';
import { Link, useNavigate, useSearchParams } from 'react-router-dom';
import { ArrowRight, CheckCheck, ClipboardCheck, Clock3, FileHeart, RefreshCw, ShieldCheck, UserRoundCheck } from 'lucide-react';
import { api, ApiError, errorMessage } from '../lib/api';
import { isVolunteer, useAuth } from '../lib/auth';
import { formatDate } from '../lib/format';
import type { ApplicationTask, ProjectTask } from '../lib/types';
import { EmptyState, LoadingState, Notice, PageHeading, StatusBadge } from '../components/ui';
import './reviews.css';

type Kind = 'application' | 'project';
type Filter = 'all' | 'pending' | 'mine';
type Task = { id: string; name: string; age: number | null; status: number; volunteer: string | null; date: string };
const PAGE_SIZE = 12;

export default function TasksPage() {
  const { user } = useAuth();
  const navigate = useNavigate();
  const [search, setSearch] = useSearchParams();
  const kind: Kind = search.get('type') === 'project' ? 'project' : 'application';
  const [filter, setFilter] = useState<Filter>('all');
  const [offset, setOffset] = useState(0);
  const [tasks, setTasks] = useState<Task[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [feedback, setFeedback] = useState('');
  const [claiming, setClaiming] = useState<string | null>(null);
  const [version, setVersion] = useState(0);
  const eligible = isVolunteer(user);

  useEffect(() => {
    if (!eligible) { setTasks([]); setLoading(false); return; }
    const controller = new AbortController();
    setLoading(true);
    setError('');
    setTasks([]);
    const path = kind === 'application' ? '/api/applications' : '/api/tasks/projects';
    api<ApplicationTask[] | ProjectTask[]>(`${path}?limit=${PAGE_SIZE}&offset=${offset}`, { signal: controller.signal })
      .then((data) => {
        if (controller.signal.aborted) return;
        const rows = data.map((item) => {
          if (kind === 'application') {
            const task = item as ApplicationTask;
            return { id: task.user_id, name: task.user_name, age: task.user_age, status: task.user_status,
              volunteer: task.current_volunteer, date: task.submitted_at };
          }
          const task = item as ProjectTask;
          return { id: task.project_id, name: task.user_name, age: task.user_age, status: task.project_status,
            volunteer: task.current_volunteer ?? null, date: task.project_create_time };
        });
        setTasks(rows);
      })
      .catch((cause: unknown) => { if (!controller.signal.aborted) setError(errorMessage(cause, '任务池暂时无法加载，请稍后重试。')); })
      .finally(() => { if (!controller.signal.aborted) setLoading(false); });
    return () => controller.abort();
  }, [eligible, kind, offset, user?.user_id, version]);

  const changeKind = (next: Kind) => {
    setOffset(0); setFilter('all'); setFeedback(''); setSearch({ type: next });
  };
  const changePage = (next: number) => { setFeedback(''); setOffset(next); };
  const claim = async (task: Task) => {
    if (claiming) return;
    setClaiming(task.id); setFeedback('');
    try {
      await api(kind === 'application' ? `/api/applications/${task.id}/claim` : `/api/projects/${task.id}/claim`, { method: 'POST' });
      navigate(`/review/${kind}/${task.id}`);
    } catch (cause: unknown) {
      setFeedback(cause instanceof ApiError && cause.status === 409
        ? '这项任务刚被其他志愿者领取了，任务池已更新，请选择其他任务。'
        : errorMessage(cause, '未能领取任务，请稍后重试。'));
      if (cause instanceof ApiError && cause.status === 409) setVersion((value) => value + 1);
    } finally { setClaiming(null); }
  };

  if (!user) return <div className="page reviews-page"><EmptyState icon={<ShieldCheck size={30} />} title="登录后，开启志愿服务" description="每一次认真核验，都是对信任的守护。登录已认证的志愿者账号，查看可以领取的审核任务。" action={<Link className="review-button primary" to="/login?next=%2Ftasks">去登录 <ArrowRight size={14} /></Link>} /></div>;
  if (!eligible) return <div className="page reviews-page"><PageHeading eyebrow="VOLUNTEER WORKSPACE" title="每一份信任，都值得认真守护" description="审核任务由已认证的志愿者与资深志愿者处理。" /><EmptyState icon={<ShieldCheck size={30} />} title={[5, 7].includes(user.user_permission) ? '志愿者认证完成后，即可领取任务' : '这里是志愿者工作台'} description={[5, 7].includes(user.user_permission) ? '你的志愿者账号需要完成平台认证。认证后，重新刷新账户状态即可进入任务池。' : '你可以浏览已审核的求助项目。若希望参与审核，请使用已认证的志愿者账号登录。'} action={<Link className="review-button primary" to="/account">查看账户状态 <ArrowRight size={14} /></Link>} /></div>;

  const pending = tasks.filter((task) => task.status === 2 || task.status === 6);
  const mine = tasks.filter((task) => task.volunteer === user.user_id && (task.status === 3 || task.status === 7));
  const visible = filter === 'pending' ? pending : filter === 'mine' ? mine : tasks;

  return <div className="page reviews-page">
    <PageHeading eyebrow="VOLUNTEER WORKSPACE" title="把一份认真，交给每一份托付" description="从资料核验到项目审核，让每一次帮助都建立在真实与信任之上。" action={<span className="review-summary-chip"><ShieldCheck size={14} />已认证志愿者</span>} />
    <div className="review-toolbar">
      <div className="review-tabs" role="tablist" aria-label="审核任务类型">
        <button role="tab" type="button" className="review-tab" aria-selected={kind === 'application'} onClick={() => changeKind('application')}><UserRoundCheck size={15} />注册资料审核</button>
        <button role="tab" type="button" className="review-tab" aria-selected={kind === 'project'} onClick={() => changeKind('project')}><FileHeart size={15} />项目审核</button>
      </div>
      <button className="review-button" type="button" disabled={loading} onClick={() => { setFeedback(''); setVersion((value) => value + 1); }}><RefreshCw size={14} className={loading ? 'review-spin' : ''} />刷新任务</button>
    </div>
    <section className="review-metrics" aria-label="当前页任务概览">
      <div className="review-metric"><span className="review-metric-icon"><ClipboardCheck size={21} /></span><div><span className="review-metric-label">当前页待领取</span><strong>{loading || error ? '—' : pending.length}</strong><small>份托付</small></div></div>
      <div className="review-metric"><span className="review-metric-icon"><Clock3 size={21} /></span><div><span className="review-metric-label">当前页我在处理</span><strong>{loading || error ? '—' : mine.length}</strong><small>份责任</small></div></div>
      <div className="review-metric"><span className="review-metric-icon"><CheckCheck size={21} /></span><div><span className="review-metric-label">当前页全部任务</span><strong>{loading || error ? '—' : tasks.length}</strong><small>份申请</small></div></div>
    </section>
    <div className="review-filters" role="group" aria-label="筛选当前页任务">
      {([['all', '全部任务'], ['pending', '待领取'], ['mine', '我的处理中']] as const).map(([value, label]) => <button className={`review-filter ${filter === value ? 'active' : ''}`} type="button" aria-pressed={filter === value} key={value} onClick={() => setFilter(value)}>{label}</button>)}
      <span className="review-page-note">筛选与数量均为当前页</span>
    </div>
    {feedback && <Notice tone="error">{feedback}</Notice>}
    {error && <Notice tone="error">{error} <button className="review-button" type="button" onClick={() => setVersion((value) => value + 1)}>重新加载</button></Notice>}
    {loading ? <LoadingState label="正在寻找需要你的一份托付…" /> : !error && <>
      {visible.length ? <div className="review-task-list">{visible.map((task) => {
        const owned = task.volunteer === user.user_id && (task.status === 3 || task.status === 7);
        return <article className="review-task-card" key={task.id} data-testid={`task-${task.id}`}>
          <div className="review-card-top"><span className="review-card-type">{kind === 'application' ? <UserRoundCheck size={13} /> : <FileHeart size={13} />}{kind === 'application' ? '患者注册资料' : '求助项目资料'} · {task.id.slice(0, 8).toUpperCase()}</span><StatusBadge status={task.status} /></div>
          <div className="review-task-ident"><span className="review-avatar" aria-hidden="true">{task.name?.slice(0, 1) || '渡'}</span><div><h2 className="review-task-name">{task.name || '一位求助者'}的{kind === 'application' ? '资料申请' : '求助项目'}</h2><p className="review-task-subtitle">{task.age != null ? `${task.age} 岁 · ` : ''}{task.status === 6 || task.status === 7 ? '补正后重新提交' : '首次提交审核'}</p></div></div>
          <p className="review-task-info">{owned ? '你已接下这份托付，请查看材料并与申请人核实信息。' : '领取后可查看申请材料，并与申请人核实相关信息。'}</p>
          <div className="review-card-footer"><span className="review-task-date"><Clock3 size={12} />{formatDate(task.date)}</span>{owned ? <Link className="review-button primary" to={`/review/${kind}/${task.id}`}>继续审核 <ArrowRight size={13} /></Link> : <button type="button" className="review-button primary" disabled={claiming !== null} onClick={() => void claim(task)}>{claiming === task.id ? '正在领取…' : '领取任务'}{claiming !== task.id && <ArrowRight size={13} />}</button>}</div>
        </article>;
      })}</div> : <EmptyState icon={<ClipboardCheck size={29} />} title={filter === 'mine' ? '当前页没有你正在处理的任务' : filter === 'pending' ? '当前页的任务都已有人守护' : '暂时没有待审核的申请'} description={filter === 'all' ? '新的申请会出现在这里。谢谢你愿意花时间，认真回应每一份求助。' : '可切换筛选或翻页查看其他任务，也可以稍后刷新任务池。'} action={filter !== 'all' ? <button className="review-button" type="button" onClick={() => setFilter('all')}>查看当前页全部任务</button> : undefined} />}
      <nav className="review-pagination" aria-label="任务分页"><button className="review-button" type="button" disabled={offset === 0} onClick={() => changePage(Math.max(0, offset - PAGE_SIZE))}>上一页</button><span>第 {Math.floor(offset / PAGE_SIZE) + 1} 页</span><button className="review-button" type="button" disabled={tasks.length < PAGE_SIZE} onClick={() => changePage(offset + PAGE_SIZE)}>下一页</button></nav>
    </>}
  </div>;
}
