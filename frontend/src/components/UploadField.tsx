import { useId, useState } from 'react';
import { FileUp, LoaderCircle } from 'lucide-react';
import { errorMessage } from '../lib/api';
import { uploadFile } from '../features/files';
import '../pages/features.css';
export default function UploadField({ purpose, projectId, onUploaded, onBusyChange, disabled = false, count = 0 }: {
  purpose: 'application' | 'project' | 'case'; projectId?: string; onUploaded: (url: string) => void; onBusyChange?: (busy: boolean) => void; disabled?: boolean; count?: number;
}) {
  const id = useId(); const [busy, setBusy] = useState(false); const [error, setError] = useState(''); const [success, setSuccess] = useState('');
  async function select(file?: File) {
    if (!file) return; setBusy(true); onBusyChange?.(true); setError(''); setSuccess('');
    try { const result = await uploadFile(file, purpose, projectId); onUploaded(result.url); setSuccess(`${result.name} 已上传，请提交表单保存这份材料。`); }
    catch (cause) { setError(errorMessage(cause)); } finally { setBusy(false); onBusyChange?.(false); }
  }
  return <div className="upload-field"><label className={`button button-secondary upload-button ${disabled || busy || count >= 20 ? 'disabled' : ''}`} htmlFor={id}>{busy ? <LoaderCircle size={16} className="spin" /> : <FileUp size={16} />}{busy ? '正在上传…' : '选择文件上传'}<input id={id} data-testid={`upload-${purpose}`} type="file" accept="image/png,image/jpeg,application/pdf,.png,.jpg,.jpeg,.pdf" disabled={disabled || busy || count >= 20} onChange={event => { void select(event.target.files?.[0]); event.target.value = ''; }} /></label><p className="field-note">支持 PNG、JPG、PDF，单个文件最多 10 MB。{purpose === 'case' ? '举报与申诉证据仅向你和运营人员开放。' : purpose === 'application' ? '证明材料仅向你、当前审核人和运营人员开放。' : '提交并审核通过的当前版本材料会随项目公开，请去除身份证号等隐私信息。'}</p>{error && <p className="feature-error" role="alert">{error}</p>}{success && <p className="feature-success" role="status">{success}</p>}</div>;
}
