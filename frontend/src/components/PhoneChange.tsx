import { useState, type FormEvent } from 'react';
import { Link } from 'react-router-dom';
import { api, errorMessage } from '../lib/api';
import { useAuth } from '../lib/auth';
import { Button, Notice } from './ui';
import { useVerification, VerificationFields } from '../features/verification';
import '../pages/features.css';
export default function PhoneChange() {
  const { user, logout } = useAuth(); const [phone, setPhone] = useState(''); const [password, setPassword] = useState('');
  const [busy, setBusy] = useState(false); const [error, setError] = useState('');
  const verification = useVerification(phone, 'phone_change');
  async function submit(event: FormEvent) {
    event.preventDefault(); setError('');
    if (phone === user?.phone_number) { setError('新手机号不能与当前手机号相同。'); return; }
    if (!verification.valid || !verification.challenge) { setError('请先验证新的手机号。'); return; }
    setBusy(true);
    try { await api('/api/me/phone', { method: 'POST', body: { phone_number: phone.trim(), challenge_id: verification.challenge.challenge_id, code: verification.code, password } }); setPassword(''); sessionStorage.setItem('crossdo.phone-changed', '1'); await logout().catch(() => null); }
    catch (cause) { setError(errorMessage(cause)); } finally { setBusy(false); }
  }
  return <section className="feature-card account-security"><h2>账户安全</h2><p className="feature-muted">更换手机号后，所有设备需要重新登录。忘记密码时，可通过注册手机号重新设置。</p><Link className="feature-subtle-link" to="/recover">找回或重置密码</Link><details className="feature-details"><summary>更换注册手机号</summary><form className="feature-form" onSubmit={submit}><div><label htmlFor="change-phone">新手机号</label><input id="change-phone" type="tel" autoComplete="tel" pattern="1[3-9][0-9]{9}" maxLength={11} required value={phone} onChange={e => setPhone(e.target.value)} disabled={busy || verification.busy} /></div><VerificationFields verification={verification} disabled={busy} /><div><label htmlFor="change-phone-password">当前登录密码</label><input id="change-phone-password" type="password" autoComplete="current-password" minLength={8} maxLength={72} required value={password} onChange={e => setPassword(e.target.value)} disabled={busy} /></div>{error && <Notice tone="error">{error}</Notice>}<Button type="submit" busy={busy}>验证并更换手机号</Button></form></details></section>;
}
