import { useState, type ButtonHTMLAttributes, type ReactNode } from 'react';
import { downloadFile, isLocalFile } from '../features/files';
import { errorMessage } from '../lib/api';
import { CheckCircle2, CircleAlert, ExternalLink, FileCheck2, FileText, LoaderCircle, NotebookPen } from 'lucide-react';
import { formatDate, safeLinks, statusLabel } from '../lib/format';
import type { ReviewRecord } from '../lib/types';

export function PageHeading({ eyebrow, title, description, action }: { eyebrow?: string; title: string; description?: string; action?: ReactNode }) {
  return <div className="page-heading"><div>{eyebrow && <p className="eyebrow">{eyebrow}</p>}<h1>{title}</h1>{description && <p className="page-description">{description}</p>}</div>{action && <div className="page-heading-action">{action}</div>}</div>;
}
export function StatusBadge({ status }: { status: number }) {
  return <span className={`badge status-${status}`}><span className="badge-dot" />{statusLabel(status)}</span>;
}
export function Notice({ children, tone = 'info' }: { children: ReactNode; tone?: 'error' | 'success' | 'info' }) {
  return <div className={`notice notice-${tone}`} role={tone === 'error' ? 'alert' : 'status'}>{tone === 'success' ? <CheckCircle2 size={17} aria-hidden="true" /> : <CircleAlert size={17} aria-hidden="true" />}<div>{children}</div></div>;
}
export function EmptyState({ icon, title, description, action }: { icon?: ReactNode; title: string; description: string; action?: ReactNode }) {
  return <div className="empty-state"><div className="empty-state-icon">{icon || <NotebookPen size={27} strokeWidth={1.4} />}</div><h3>{title}</h3><p>{description}</p>{action && <div className="empty-state-action">{action}</div>}</div>;
}
export function LoadingState({ label = '正在读取最新资料…' }: { label?: string }) { return <div className="loading-state" role="status"><LoaderCircle className="spin" size={22} aria-hidden="true" /><span>{label}</span></div>; }
export function Button({ children, variant = 'primary', busy = false, disabled, className = '', ...props }: ButtonHTMLAttributes<HTMLButtonElement> & { variant?: 'primary' | 'secondary' | 'ghost' | 'danger'; busy?: boolean }) {
  return <button {...props} disabled={disabled || busy} aria-busy={busy || undefined} className={`button button-${variant} ${className}`}>{busy && <LoaderCircle size={16} className="spin" aria-hidden="true" />}{children}</button>;
}
export function Field({ label, htmlFor, hint, children }: { label: string; htmlFor: string; hint?: string; children: ReactNode }) { return <div className="field"><label htmlFor={htmlFor}>{label}</label>{children}{hint && <p className="field-hint">{hint}</p>}</div>; }
function materialName(link: string, index: number): string {
  if (isLocalFile(link)) return `上传材料 ${index + 1}`;
  try { return decodeURIComponent(new URL(link).pathname.split('/').filter(Boolean).pop() || '') || `材料 ${index + 1}`; } catch { return `材料 ${index + 1}`; }
}
export function MaterialLinks({ links, label = '相关材料' }: { links: string[]; label?: string }) {
  const [busy, setBusy] = useState<string | null>(null); const [error, setError] = useState('');
  const valid = safeLinks(links);
  async function download(link: string) { setBusy(link); setError(''); try { await downloadFile(link); } catch (cause) { setError(errorMessage(cause)); } finally { setBusy(null); } }
  if (!valid.length) return <p className="muted">暂无可查看的文件材料</p>;
  return <><ul className="material-links" aria-label={label}>{valid.map((link, index) => <li key={`${link}-${index}`}>{isLocalFile(link) ? <button className="file-download" type="button" disabled={busy !== null} onClick={() => void download(link)}><span className="file-icon"><FileText size={18} /></span><span className="file-name">{materialName(link, index)}<small>{busy === link ? '正在读取…' : '下载文件材料'}</small></span><ExternalLink size={14} aria-hidden="true" /></button> : <a href={link} target="_blank" rel="noopener noreferrer"><span className="file-icon"><FileText size={18} /></span><span className="file-name">{materialName(link, index)}<small>查看文件材料</small></span><ExternalLink size={14} aria-hidden="true" /></a>}</li>)}</ul>{error && <p className="feature-error" role="alert">{error}</p>}</>;
}

const decisionLabels: Record<string, string> = { submit: '提交审核资料', claim: '志愿者领取任务', approve: '审核通过', correction: '请补充相关材料', unreachable: '暂未联系成功' };
export function ReviewHistory({ records }: { records?: ReviewRecord[] }) {
  return <div className="review-history"><h3><FileCheck2 size={17} />审核记录</h3>{!records?.length ? <p className="muted">提交资料后，审核进展会记录在这里。</p> : <ol className="timeline">{records.map((record, index) => <li key={`${record.time}-${index}`}><span className={`timeline-dot ${record.decision === 'approve' ? 'approved' : ''}`} /><div className="timeline-heading"><strong>{decisionLabels[record.decision] || '审核进展'}</strong><time>{formatDate(record.time)}</time></div><p>{record.decision === 'submit' ? '资料已提交，等待志愿者认真核验。' : record.decision === 'claim' ? '志愿者已领取本次审核任务。' : record.note}</p></li>)}</ol>}</div>;
}
