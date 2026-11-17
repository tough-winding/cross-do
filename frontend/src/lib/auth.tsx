import { createContext, useCallback, useContext, useEffect, useRef, useState } from 'react';
import type { ReactNode } from 'react';
import { api, ApiError, AUTH_EVENT, errorMessage, savedSession, SESSION_KEY } from './api';
import type { Registration, User } from './types';
interface AuthContextValue {
  user: User | null; token: string | null; loading: boolean; expired: boolean;
  restorationError: string; retrySession: () => Promise<void>;
  login: (phone: string, password: string) => Promise<void>;
  register: (input: Registration) => Promise<void>;
  refreshUser: () => Promise<User | null>; logout: () => Promise<void>;
}
const AuthContext = createContext<AuthContextValue | null>(null);
export function AuthProvider({ children }: { children: ReactNode }) {
  const [token, setToken] = useState<string | null>(() => savedSession()?.token || null);
  const [user, setUser] = useState<User | null>(null);
  const [loading, setLoading] = useState(!!token);
  const [expired, setExpired] = useState(false);
  const [restorationError, setRestorationError] = useState('');
  const revision = useRef(0);
  const clear = useCallback(() => {
    revision.current++; sessionStorage.removeItem(SESSION_KEY); setToken(null); setUser(null); setRestorationError('');
  }, []);
  const refreshUser = useCallback(async () => {
    const session = savedSession();
    if (!session) { clear(); return null; }
    const generation = revision.current;
    const profile = await api<User>('/api/me');
    if (revision.current !== generation || savedSession()?.token !== session.token) return null;
    setUser(profile); setRestorationError(''); return profile;
  }, [clear]);
  const retrySession = useCallback(async () => {
    setLoading(true); setRestorationError('');
    try { await refreshUser(); }
    catch (error) { if (savedSession()) setRestorationError(errorMessage(error, '暂时无法恢复登录状态，请稍后重试。')); }
    finally { setLoading(false); }
  }, [refreshUser]);
  useEffect(() => {
    const controller = new AbortController(); const session = savedSession(); const generation = revision.current;
    if (session) api<User>('/api/me', { signal: controller.signal }).then((profile) => {
      if (!controller.signal.aborted && revision.current === generation && savedSession()?.token === session.token) setUser(profile);
    }).catch((error) => {
      if (controller.signal.aborted || revision.current !== generation) return;
      if (error instanceof ApiError && error.status === 401 || !savedSession()) clear();
      else setRestorationError(errorMessage(error, '暂时无法恢复登录状态，请稍后重试。'));
    }).finally(() => { if (!controller.signal.aborted) setLoading(false); });
    else setLoading(false);
    const onExpired = () => { clear(); setExpired(true); };
    window.addEventListener(AUTH_EVENT, onExpired);
    return () => { controller.abort(); window.removeEventListener(AUTH_EVENT, onExpired); };
  }, [clear]);
  useEffect(() => {
    if (!token) return;
    const session = savedSession();
    if (!session) { clear(); setExpired(true); return; }
    const timer = window.setTimeout(() => {
      if (savedSession()?.token === token || !savedSession()) { clear(); setExpired(true); }
    }, Math.min(Math.max(0, session.expiresAt - Date.now()), 2_147_483_647));
    return () => window.clearTimeout(timer);
  }, [token, clear]);
  const login = useCallback(async (phone: string, password: string) => {
    const generation = ++revision.current;
    const result = await api<{ token: string; expires_in: number; user: User }>('/api/auth/login', { method: 'POST', body: { phone_number: phone, password } });
    if (revision.current !== generation) throw new Error('登录操作已取消，请重新登录。');
    sessionStorage.setItem(SESSION_KEY, JSON.stringify({ token: result.token, expiresAt: Date.now() + result.expires_in * 1000 }));
    setToken(result.token); setUser(result.user); setExpired(false); setRestorationError('');
  }, []);
  const register = useCallback(async (input: Registration) => {
    await api<User>('/api/auth/register', { method: 'POST', body: input });
    try { await login(input.phone_number, input.password); }
    catch { throw new Error('账号已创建，但暂时无法自动登录。请切换至手机号登录后重试。'); }
  }, [login]);
  const logout = useCallback(async () => {
    const revocation = savedSession() ? api<null>('/api/auth/logout', { method: 'POST' }) : Promise.resolve(null);
    clear(); setExpired(false); await revocation;
  }, [clear]);
  return <AuthContext.Provider value={{ user, token, loading, expired, restorationError, retrySession, login, register, refreshUser, logout }}>{children}</AuthContext.Provider>;
}
export function useAuth() { const context = useContext(AuthContext); if (!context) throw new Error('AuthProvider missing'); return context; }
export function isVolunteer(user: User | null): boolean { return !!user && [5, 7].includes(user.user_permission) && user.user_status === 0 && user.real_name_authentication === 1; }
export function isPatient(user: User | null): boolean { return user?.user_permission === 3; }
