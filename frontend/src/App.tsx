import type { ReactNode } from 'react';
import { HashRouter, Link, Navigate, Route, Routes, useLocation } from 'react-router-dom';
import { AuthProvider, useAuth } from './lib/auth';
import Shell from './components/Shell';
import { Button, EmptyState, LoadingState, Notice } from './components/ui';
import HomePage from './pages/HomePage';
import AccountPage from './pages/AccountPage';
import AuthPage from './pages/AuthPage';
import ApplicationPage from './pages/ApplicationPage';
import ProjectsPage from './pages/ProjectsPage';
import ProjectDetailPage from './pages/ProjectDetailPage';
import MyProjectPage from './pages/MyProjectPage';
import TasksPage from './pages/TasksPage';
import ReviewPage from './pages/ReviewPage';
import AdminPage from './pages/AdminPage';
import NotificationsPage from './pages/NotificationsPage';
import CasesPage from './pages/CasesPage';
import RecoveryPage from './pages/RecoveryPage';
import DonationsPage from './pages/DonationsPage';
function Protected({ children }: { children: ReactNode }) {
  const { user, token, loading, restorationError, retrySession } = useAuth(); const location = useLocation();
  if (loading) return <LoadingState label="正在恢复您的登录状态…" />;
  if (token && restorationError) return <div className="page panel stack"><Notice tone="error">{restorationError}</Notice><div><Button onClick={() => void retrySession()}>重新恢复登录状态</Button></div></div>;
  if (!user) return <Navigate to={`/login?next=${encodeURIComponent(location.pathname + location.search)}`} replace />;
  return children;
}
export default function App() {
  return <HashRouter><AuthProvider><Routes><Route element={<Shell />}>
    <Route index element={<HomePage />} /><Route path="projects" element={<ProjectsPage />} /><Route path="projects/:projectId" element={<Protected><ProjectDetailPage /></Protected>} />
    <Route path="login" element={<AuthPage />} /><Route path="register" element={<AuthPage />} />
    <Route path="recover" element={<RecoveryPage />} />
    <Route path="admin" element={<Protected><AdminPage /></Protected>} />
    <Route path="notifications" element={<Protected><NotificationsPage /></Protected>} />
    <Route path="cases" element={<Protected><CasesPage /></Protected>} />
    <Route path="donations" element={<Protected><DonationsPage /></Protected>} />
    <Route path="application" element={<Protected><ApplicationPage /></Protected>} /><Route path="my-project" element={<Protected><MyProjectPage /></Protected>} />
    <Route path="tasks" element={<Protected><TasksPage /></Protected>} /><Route path="review/:kind/:id" element={<Protected><ReviewPage /></Protected>} /><Route path="account" element={<Protected><AccountPage /></Protected>} />
    <Route path="*" element={<div className="page panel"><EmptyState title="这段航程还未开启" description="页面可能已移动，回到首页继续了解渡舟。" action={<Link className="button button-primary" to="/">返回首页</Link>} /></div>} />
  </Route></Routes></AuthProvider></HashRouter>;
}
