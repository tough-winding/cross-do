import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { ArrowRight, ArrowUpRight, Check, ClipboardCheck, FileHeart, Flower2, HeartHandshake, LifeBuoy, LockKeyhole, ShieldCheck, Sparkles } from 'lucide-react';
import { api, errorMessage } from '../lib/api';
import { isVolunteer, useAuth } from '../lib/auth';
import type { ProjectSummary } from '../lib/types';
import { money, projectTitle } from '../lib/format';
import { EmptyState, LoadingState, Notice } from '../components/ui';

function RiverIllustration() {
  return <svg className="river-illustration" viewBox="0 0 520 330" fill="none" aria-hidden="true"><defs><linearGradient id="river" x1="120" y1="100" x2="400" y2="350" gradientUnits="userSpaceOnUse"><stop stopColor="#638d76" stopOpacity=".2" /><stop offset="1" stopColor="#a7c4a0" stopOpacity=".6" /></linearGradient></defs><circle cx="366" cy="88" r="48" fill="#ced89a" opacity=".9" /><circle cx="366" cy="88" r="61" stroke="#a6c18f" strokeOpacity=".2" /><path d="M59 214C106 119 145 146 187 172c48 29 76-16 109-47 40-38 100-13 157 68l49 137H6z" fill="#345f4e" /><path d="M123 267c64-84 102-68 158-43 45 20 98-63 163-71l76 177H76z" fill="#547c5e" opacity=".7" /><path d="M113 330c70-77 156-64 184-110 22-36-39-52-40-79-1-18 9-26 30-37-48 13-66 32-41 58 30 31 16 40-19 55C183 236 82 247 23 330z" fill="url(#river)" /><path d="M75 285c80-16 108-2 148-25M83 310c83-18 119-6 164-28M276 195c15 2 29 0 37-6M257 221c23 0 42-5 52-14" stroke="#b8d1a2" strokeOpacity=".45" strokeLinecap="round" /><path d="M273 236h87l-21 19h-48z" fill="#e9dfae" /><path d="M318 159v69h-52z" fill="#f1edd3" /><path d="M326 181v47h29z" fill="#adc6a1" /><path d="M318 156v80" stroke="#f5edd3" strokeWidth="3" strokeLinecap="round" /><path d="m350 244 25 21" stroke="#e9dfae" strokeWidth="3" strokeLinecap="round" /><path d="M296 265c12-3 23-3 35-2M288 276c13-2 35-3 47-2" stroke="#b8d1a2" strokeOpacity=".6" strokeLinecap="round" /><path d="M414 99c5-6 9-7 15-1M390 75c4-4 7-4 11-1" stroke="#264d40" strokeWidth="1.5" strokeLinecap="round" /><path d="M456 294v-74m-18 43 18 12 17-20m-31-10 14 9 12-15" stroke="#acc194" strokeOpacity=".5" strokeWidth="2" /><circle cx="155" cy="118" r="3" fill="#a4be8a" opacity=".5" /><circle cx="212" cy="79" r="2" fill="#a4be8a" opacity=".5" /></svg>;
}
export default function HomePage() {
  const { user } = useAuth();
  const [projects, setProjects] = useState<ProjectSummary[]>([]);
  const [loading, setLoading] = useState(!!user);
  const [error, setError] = useState('');
  const [refresh, setRefresh] = useState(0);
  useEffect(() => {
    if (!user) { setProjects([]); setLoading(false); return; }
    const controller = new AbortController(); setLoading(true); setError('');
    api<ProjectSummary[]>('/api/projects?summary=true&limit=3&offset=0', { signal: controller.signal }).then(setProjects).catch((reason) => { if (!controller.signal.aborted) setError(errorMessage(reason)); }).finally(() => { if (!controller.signal.aborted) setLoading(false); });
    return () => controller.abort();
  }, [user, refresh]);
  const destination = !user ? '/register' : user.user_permission === 3 ? '/application' : isVolunteer(user) ? '/tasks' : '/projects';
  return <div className="page home-page">
    <div className="home-intro"><div><p className="eyebrow">A LITTLE KINDNESS, A LONG WAY</p><h1>{user ? `${user.user_name}，欢迎回到渡舟` : '让微小的善意，汇成向前的力量'}</h1><p>每一份求助都被认真倾听，每一次帮助都有迹可循。</p></div><span className="home-intro-symbol"><Flower2 size={30} strokeWidth={1.2} /></span></div>
    <section className="hero-panel"><div className="hero-copy"><span className="hero-kicker"><Sparkles size={13} /> 浊水亦可渡，微光终成舟</span><h2>让每一份善意，<br />都抵达需要的地方。</h2><p>连接需要帮助的人与愿意伸出援手的你，<br className="desktop-break" />让希望在真实与信任中，慢慢生长。</p><Link className="button hero-button" to={destination}>{!user ? '和我们一起，让善意启航' : user.user_permission === 3 ? '查看我的求助申请' : isVolunteer(user) ? '前往志愿审核工作台' : '发现需要帮助的项目'}<ArrowUpRight size={17} /></Link></div><RiverIllustration /><div className="hero-caption"><span />一叶渡舟 · 一份希望</div></section>
    <section className="quick-actions" aria-label="开始参与">{(user?.user_permission === 3 ? [
      { to: '/application', title: '我的求助资料', text: '完善求助材料，查看核验进度', icon: FileHeart, color: 'sage' },
      { to: '/my-project', title: '我的求助项目', text: '管理项目资料，记录康复进展', icon: HeartHandshake, color: 'sand' },
      { to: '/projects', title: '看看同行的人', text: '了解真实求助，陪伴每段旅程', icon: ClipboardCheck, color: 'blue' },
    ] : user && [5, 7].includes(user.user_permission) ? [
      { to: '/tasks', title: '我的审核工作台', text: '领取待审资料，守护每份信任', icon: FileHeart, color: 'sage' },
      { to: '/projects', title: '与希望同行', text: '关注求助项目，了解最新进展', icon: HeartHandshake, color: 'sand' },
      { to: '/account', title: '我的志愿身份', text: '查看账户资料与认证情况', icon: ClipboardCheck, color: 'blue' },
    ] : user ? [
      { to: '/projects', title: '发现真实求助', text: '了解已审核的患者求助', icon: FileHeart, color: 'sage' },
      { to: '/projects', title: '与希望同行', text: '关注治疗记录，陪伴每段旅程', icon: HeartHandshake, color: 'sand' },
      { to: '/account', title: '我的渡舟账户', text: '查看参与身份与账户资料', icon: ClipboardCheck, color: 'blue' },
    ] : [
      { to: '/register?role=3', title: '我需要帮助', text: '提交求助资料，让希望被看见', icon: FileHeart, color: 'sage' },
      { to: '/login?next=%2Fprojects', title: '我愿意同行', text: '了解真实求助，关注项目进展', icon: HeartHandshake, color: 'sand' },
      { to: '/register?role=5', title: '成为志愿者', text: '用认真与耐心，守护每份信任', icon: ClipboardCheck, color: 'blue' },
    ]).map(({to,title,text,icon:Icon,color}) => <Link className="quick-action" to={to} key={title}><span className={`quick-icon icon-${color}`}><Icon size={23} strokeWidth={1.6} /></span><div><h3>{title}</h3><p>{text}</p></div><ArrowUpRight size={17} /></Link>)}</section>
    <section className="home-projects"><div className="section-heading"><div><span className="section-overline">与希望同行</span><h2>每一份求助，都值得被看见 <span className="tiny-leaf">✳</span></h2></div><Link to="/projects" className="text-link">查看全部项目 <ArrowRight size={16} /></Link></div>{loading ? <div className="panel"><LoadingState /></div> : error ? <div className="panel home-error"><Notice tone="error">{error}</Notice><button className="button button-secondary" onClick={() => setRefresh((value) => value + 1)}>重新读取项目</button></div> : projects.length ? <div className="home-project-grid">{projects.map((project, index) => <Link to={`/projects/${project.project_id}`} className="home-project-card" key={project.project_id}><div className={`project-cover cover-${index % 3}`}><Flower2 size={65} strokeWidth={.8} /><span><ShieldCheck size={13} /> 志愿者已核验</span></div><div className="home-project-card-body"><span className="project-card-caption">医疗求助 · 正在进行</span><h3>{projectTitle(project)}</h3><p>{project.summary || '了解这份求助的详细资料与最新进展。'}</p><div className="home-card-funding"><div><small>已收到</small><strong>{money(project.current_donations_received)}</strong></div><ArrowUpRight size={18} /></div></div></Link>)}</div> : <div className="panel home-project-empty"><EmptyState icon={user ? <LifeBuoy size={30} strokeWidth={1.2} /> : <LockKeyhole size={28} strokeWidth={1.3} />} title={user ? '希望正在路上' : '登录后，走近每一份真实的求助'} description={user ? '审核通过的求助项目会在这里公开。每一个等待，都值得被认真对待。' : '查看经过志愿者审核的项目，了解病情记录与最新进展。'} action={<Link to={user ? '/projects' : '/login?next=%2Fprojects'} className="button button-secondary">{user ? '前往项目广场' : '登录查看项目'}<ArrowRight size={15} /></Link>} /></div>}</section>
    <section className="trust-section"><div className="trust-lead"><ShieldCheck size={23} strokeWidth={1.4} /><div><h3>把信任，落在每一步</h3><p>善意不止于出发，也在于每一步的认真。</p></div></div><div className="trust-points"><span><Check size={15} />求助资料逐项审核</span><span><Check size={15} />病情进展持续记录</span><span><Check size={15} />审核过程完整留痕</span></div></section>
  </div>;
}
