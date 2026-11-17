export const SESSION_KEY = 'crossdo.session';
export const AUTH_EVENT = 'crossdo:session-expired';
interface SavedSession { token: string; expiresAt: number }
export function savedSession(): SavedSession | null {
  try {
    const value = JSON.parse(sessionStorage.getItem(SESSION_KEY) || 'null');
    if (!value || typeof value.token !== 'string' || typeof value.expiresAt !== 'number' || value.expiresAt <= Date.now()) {
      sessionStorage.removeItem(SESSION_KEY); return null;
    }
    return value;
  } catch { sessionStorage.removeItem(SESSION_KEY); return null; }
}
export class ApiError extends Error {
  status: number;
  constructor(status: number, message: string) { super(message); this.name = 'ApiError'; this.status = status; }
}
export async function api<T>(path: string, options: { method?: string; body?: unknown; signal?: AbortSignal } = {}): Promise<T> {
  const token = savedSession()?.token;
  const headers: Record<string, string> = { Accept: 'application/json' };
  if (options.body !== undefined) headers['Content-Type'] = 'application/json';
  if (token) headers.Authorization = `Bearer ${token}`;
  let response: Response;
  try {
    response = await fetch(path, { method: options.method || 'GET', headers, body: options.body === undefined ? undefined : JSON.stringify(options.body), signal: options.signal, cache: 'no-store' });
  } catch (error) {
    if (error instanceof DOMException && error.name === 'AbortError') throw error;
    throw new ApiError(0, '暂时无法连接，请检查网络后重试。');
  }
  const payload = response.status === 204 ? null : await response.json().catch(() => null);
  if (!response.ok) {
    if (response.status === 401 && !path.startsWith('/api/auth/') && (token ? savedSession()?.token === token : !savedSession())) {
      sessionStorage.removeItem(SESSION_KEY); window.dispatchEvent(new Event(AUTH_EVENT));
    }
    throw new ApiError(response.status, payload?.message || '请求未完成，请稍后重试。');
  }
  return payload as T;
}
export function errorMessage(error: unknown, fallback = '操作未完成，请稍后重试。'): string {
  if (!(error instanceof Error)) return fallback;
  const message = error.message;
  if (/Record already exists|Phone number already/i.test(message)) return '这个手机号已注册，请直接登录。';
  if (/already has a project/i.test(message)) return '您已有一个项目，请前往“我的项目”查看。';
  if (/not available|not pending|not claimable|not under review|Concurrent change/i.test(message)) return '这项任务的状态已经变化，请刷新后查看最新进展。';
  if (/assigned volunteer|assigned reviewer|only the assigned/i.test(message)) return '这项任务由其他志愿者负责，您无法进行审核。';
  if (/verified|active.*patient|active.*volunteer|account is not active/i.test(message)) return '当前账号尚未通过认证，或暂时无法使用此功能。';
  if (/Record not found/i.test(message)) return '暂未找到这条记录。';
  if (/[\u4e00-\u9fff]/.test(message)) return message;
  if (error instanceof ApiError && error.status === 403) return '您没有查看或操作这项内容的权限。';
  if (error instanceof ApiError && error.status === 409) return '记录状态已经变化，请刷新后再试。';
  if (error instanceof ApiError && error.status >= 500) return '服务暂时不可用，请稍后重试。';
  if (error instanceof ApiError && error.status === 400) return '请检查所填资料、文件链接和金额是否正确。';
  return fallback;
}
