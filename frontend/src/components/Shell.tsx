import { useEffect, useState } from 'react';
import { Link, NavLink, Outlet, useLocation, useNavigate } from 'react-router-dom';
import { ArrowUpRight, Bell, ChevronDown, ChevronRight, ClipboardCheck, Compass, FileHeart, HeartHandshake, House, LogOut, Menu, ShieldCheck, UserRound, Wallet, MessageSquareWarning, Settings, X } from 'lucide-react';
import { useAuth } from '../lib/auth';
import { roleLabel } from '../lib/format';
import { api } from '../lib/api';

export function BoatMark({ className = '' }: { className?: string }) {
  return <svg viewBox="0 0 48 48" className={className} fill="none" aria-hidden="true"><path d="M9 29h32l-7 9H17z" fill="currentColor" /><path d="M25 7v18H12z" fill="currentColor" /><path d="M29 13v12h10z" fill="currentColor" opacity=".55" /><path d="M7 42c6 3 11-3 17 0s11-3 17 0" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" /></svg>;
}
export default function Shell() {
  const { user, logout, refreshUser } = useAuth();
  const [mobileOpen, setMobileOpen] = useState(false);
  const [unread, setUnread] = useState(0);
  const location = useLocation();
  const navigate = useNavigate();
  useEffect(() => { setMobileOpen(false); window.scrollTo({ top: 0 }); }, [location.pathname]);
  useEffect(() => { if (user?.user_id) void refreshUser().catch(() => null); }, [location.pathname, user?.user_id, refreshUser]);
  useEffect(() => {
    let alive = true;
    const load = () => { if (user) void api<{count: number}>('/api/notifications/unread-count').then(value => { if (alive) setUnread(value.count); }).catch(() => null); else setUnread(0); };
    load(); window.addEventListener('crossdo:notifications-updated', load);
    return () => { alive = false; window.removeEventListener('crossdo:notifications-updated', load); };
  }, [user?.user_id, location.pathname]);
  useEffect(() => { document.body.style.overflow = mobileOpen ? 'hidden' : ''; return () => { document.body.style.overflow = ''; }; }, [mobileOpen]);
  const title = location.pathname.startsWith('/projects/') ? '项目详情' : ({ '/': '首页', '/projects': '发现项目', '/application': '我的申请', '/my-project': '我的项目', '/tasks': '审核工作台', '/account': '账户资料', '/login': '登录账户', '/register': '加入渡舟', '/recover': '找回密码', '/admin': '运营管理', '/notifications': '消息通知', '/cases': '举报与申诉', '/donations': '我的捐赠' } as Record<string, string>)[location.pathname] || '审核详情';
  const exit = async () => { try { await logout(); } catch { /* Local session is cleared even if the network is unavailable. */ } navigate('/'); };
  const nav = <>
    <span className="nav-group-label">善意，从这里出发</span>
    <NavLink end to="/" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><House size={19} />首页<span className="nav-end">›</span></NavLink>
    <NavLink to="/projects" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><Compass size={19} />发现项目<span className="nav-end">›</span></NavLink>
    {user && <><span className="nav-group-label nav-group-spaced">我的渡舟</span>{user.user_permission === 3 && <>
      <NavLink to="/application" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><FileHeart size={19} />我的申请<span className="nav-end">›</span></NavLink>
      <NavLink to="/my-project" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><HeartHandshake size={19} />我的项目<span className="nav-end">›</span></NavLink>
    </>}{[5, 7].includes(user.user_permission) && <NavLink to="/tasks" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><ClipboardCheck size={19} />审核工作台<span className="nav-end">›</span></NavLink>}
      <NavLink to="/notifications" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><Bell size={19} />消息通知<span className="nav-end">{unread ? Math.min(unread, 99) : '›'}</span></NavLink>
      <NavLink to="/donations" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><Wallet size={19} />我的捐赠<span className="nav-end">›</span></NavLink>
      <NavLink to="/cases" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><MessageSquareWarning size={19} />举报与申诉<span className="nav-end">›</span></NavLink>
      {user.user_permission === 1 && <NavLink to="/admin" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><Settings size={19} />运营管理<span className="nav-end">›</span></NavLink>}
      <NavLink to="/account" className={({ isActive }) => `nav-item ${isActive ? 'active' : ''}`}><UserRound size={19} />账户资料<span className="nav-end">›</span></NavLink>
    </>}
  </>;
  return <div className="app-shell">
    <a className="skip-link" href="#main-content" onClick={(event) => { event.preventDefault(); document.getElementById('main-content')?.focus(); }}>跳至主要内容</a>
    {mobileOpen && <button className="mobile-scrim" aria-label="关闭导航遮罩" onClick={() => setMobileOpen(false)} />}
    <aside className={`sidebar ${mobileOpen ? 'is-open' : ''}`} aria-label="主导航">
      <Link to="/" className="brand" aria-label="渡舟首页"><span className="brand-mark"><BoatMark /></span><span><strong>渡舟<span className="brand-dot">.</span></strong><small>CROSSDO</small></span></Link>
      <button className="icon-button mobile-close" aria-label="关闭导航" onClick={() => setMobileOpen(false)}><X size={20} /></button>
      <nav>{nav}</nav>
      <div className="sidebar-bottom"><div className="sidebar-note"><span className="note-leaf">✳</span><p>一个人的微光，<br />也能照亮一段航程。</p><Link to={user ? '/projects' : '/register'}>让善意启航 <ArrowUpRight size={14} /></Link></div><div className="sidebar-foot"><ShieldCheck size={14} /><span>每一份信任，认真以待</span></div></div>
    </aside>
    <div className="app-body">
      <header className="topbar"><div className="breadcrumb"><button className="icon-button mobile-menu" aria-label="打开导航" aria-expanded={mobileOpen} onClick={() => setMobileOpen(true)}><Menu size={21} /></button><span className="breadcrumb-home">渡舟公益</span><ChevronRight size={13} /><span>{title}</span></div><div className="topbar-actions"><span className="header-motto"><i />让善意有所抵达</span>{user ? <><Link className="profile-chip" to="/account"><span className="avatar">{user.user_name.slice(0, 1)}</span><span><strong>{user.user_name}</strong><small>{roleLabel(user.user_permission)}</small></span><ChevronDown size={13} /></Link><button className="icon-button logout-button" onClick={exit} aria-label="退出登录" title="退出登录"><LogOut size={17} /></button></> : <Link to="/login" className="button button-primary header-login">登录 / 注册 <ArrowUpRight size={15} /></Link>}</div></header>
      <main className="main-content" id="main-content" tabIndex={-1}><Outlet /></main>
      <footer className="app-footer"><span>渡舟 CROSSDO <span className="footer-separator">/</span> 愿予清风助渡舟</span><span>真实记录 · 志愿审核 · 善意同行</span></footer>
    </div>
  </div>;
}
