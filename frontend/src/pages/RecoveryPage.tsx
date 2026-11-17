import { useState, type FormEvent } from 'react';
import { Link } from 'react-router-dom';
import { ArrowLeft, KeyRound } from 'lucide-react';
import { api, errorMessage } from '../lib/api';
import { useAuth } from '../lib/auth';
import { Button, Notice, PageHeading } from '../components/ui';
import { useVerification, VerificationFields } from '../features/verification';
import './features.css';
export default function RecoveryPage() {
  const { logout } = useAuth(); const [phone, setPhone] = useState(''); const [password, setPassword] = useState(''); const [confirm, setConfirm] = useState('');
  const verification = useVerification(phone, 'password_reset');
  const [busy, setBusy] = useState(false); const [error, setError] = useState(''); const [complete, setComplete] = useState(false);
  async function submit(event: FormEvent) {
    event.preventDefault(); setError('');
    if (!verification.valid || !verification.challenge) { setError('请先获取并填写手机验证码。'); return; }
    if (password !== confirm) { setError('两次填写的密码不一致。'); return; }
    if (new TextEncoder().encode(password).length > 72) { setError('密码不能超过 72 个 UTF-8 字节。'); return; }
    setBusy(true);
    try { await api('/api/auth/password-reset', { method: 'POST', body: { phone_number: phone.trim(), challenge_id: verification.challenge.challenge_id, code: verification.code, password } }); setPassword(''); setConfirm(''); setComplete(true); await logout().catch(() => null); }
    catch (cause) { setError(errorMessage(cause)); } finally { setBusy(false); }
  }
  return <div className="page feature-page recovery-page"><Link className="project-breadcrumb" to="/login"><ArrowLeft size={15} />返回登录</Link><PageHeading eyebrow="ACCOUNT RECOVERY" title="重新找回你的渡舟" description="验证注册手机号后设置新密码，原有登录会话会全部失效。" /><section className="feature-card">{complete ? <div className="feature-stack"><Notice tone="success">密码已更新，请使用新密码重新登录。</Notice><Link className="button button-primary" to="/login">返回登录</Link></div> : <form className="feature-form" onSubmit={submit}><div><label htmlFor="recovery-phone">注册手机号</label><input id="recovery-phone" type="tel" autoComplete="username" pattern="1[3-9][0-9]{9}" maxLength={11} required value={phone} onChange={e => setPhone(e.target.value)} disabled={busy || verification.busy} /></div><VerificationFields verification={verification} disabled={busy} /><div><label htmlFor="recovery-password">新密码</label><input id="recovery-password" type="password" autoComplete="new-password" minLength={8} maxLength={72} required value={password} onChange={e => setPassword(e.target.value)} disabled={busy} /></div><div><label htmlFor="recovery-confirm">再次填写新密码</label><input id="recovery-confirm" type="password" autoComplete="new-password" minLength={8} maxLength={72} required value={confirm} onChange={e => setConfirm(e.target.value)} disabled={busy} /></div>{error && <Notice tone="error">{error}</Notice>}<Button type="submit" busy={busy}><KeyRound size={16} />更新密码</Button></form>}</section></div>;
}
