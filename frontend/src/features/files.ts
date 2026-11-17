import { ApiError, AUTH_EVENT, savedSession, SESSION_KEY } from '../lib/api';
export interface UploadedFile { id: string; name: string; size: number; mime_type: string; url: string }
export const isLocalFile = (value: string): boolean => /^\/api\/files\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(value);
export function validReference(value: string): boolean {
  if (isLocalFile(value)) return true;
  try { return value.length <= 2048 && ['http:', 'https:'].includes(new URL(value).protocol); } catch { return false; }
}
export function references(value: string): string[] {
  const links = value.split('\n').map(link => link.trim()).filter(Boolean);
  if (links.length > 20 || links.some(link => !validReference(link))) throw new Error('最多提供 20 份材料，请上传文件或填写有效的 HTTP(S) 链接。');
  return links;
}
async function fileRequest(path: string, init: RequestInit): Promise<Response> {
  const token = savedSession()?.token;
  const headers = new Headers(init.headers);
  if (token) headers.set('Authorization', `Bearer ${token}`);
  let response: Response;
  try { response = await fetch(path, { ...init, headers, cache: 'no-store' }); }
  catch { throw new ApiError(0, '文件服务暂时无法连接，请稍后重试。'); }
  if (!response.ok) {
    if (response.status === 401 && (token ? savedSession()?.token === token : !savedSession())) {
      sessionStorage.removeItem(SESSION_KEY); window.dispatchEvent(new Event(AUTH_EVENT));
    }
    const body = await response.json().catch(() => null);
    throw new ApiError(response.status, body?.message || '文件操作未完成，请稍后重试。');
  }
  return response;
}
export async function uploadFile(file: File, purpose: 'application' | 'project' | 'case', projectId?: string): Promise<UploadedFile> {
  if (!file.size || file.size > 10 * 1024 * 1024) throw new Error('文件大小须在 1 字节到 10 MB 之间。');
  if (!/\.(png|jpe?g|pdf)$/i.test(file.name)) throw new Error('请选择 PNG、JPG 或 PDF 文件。');
  const form = new FormData(); form.append('file', file); form.append('purpose', purpose);
  if (projectId) form.append('project_id', projectId);
  const response = await fileRequest('/api/files', { method: 'POST', body: form });
  return response.json() as Promise<UploadedFile>;
}
export async function downloadFile(path: string): Promise<void> {
  if (!isLocalFile(path)) throw new Error('文件地址不正确。');
  const response = await fileRequest(path, { method: 'GET' });
  const blob = await response.blob();
  const disposition = response.headers.get('Content-Disposition') || '';
  const encodedName = /filename\*=UTF-8''([^;]+)/i.exec(disposition)?.[1];
  let name = /filename="?([^";]+)"?/i.exec(disposition)?.[1] || '渡舟材料';
  if (encodedName) { try { name = decodeURIComponent(encodedName); } catch { /* Keep safe fallback. */ } }
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a'); anchor.href = url; anchor.download = name.replace(/[\\/\x00-\x1f]/g, '_');
  document.body.append(anchor); anchor.click(); anchor.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 10_000);
}
