import { useEffect, useRef, useState } from 'react';
import { api, errorMessage } from '../lib/api';
import { Notice } from '../components/ui';
import '../pages/features.css';
export interface VerificationSettings { sms_mode: 'disabled' | 'development' | 'gateway'; phone_verification_required: boolean }
interface Challenge { challenge_id: string; expires_in: number; resend_after: number; development_code?: string }
export function useVerification(phone: string, purpose: 'register' | 'password_reset' | 'phone_change') {
  const currentPhone = useRef(phone.trim()); currentPhone.current = phone.trim();
  const [settings, setSettings] = useState<VerificationSettings | null>(null);
  const [challenge, setChallenge] = useState<Challenge | null>(null);
  const [code, setCode] = useState(''); const [busy, setBusy] = useState(false); const [error, setError] = useState('');
  const [cooldown, setCooldown] = useState(0);
  useEffect(() => { const controller = new AbortController(); api<VerificationSettings>('/api/auth/settings', { signal: controller.signal }).then(setSettings).catch(cause => { if (!controller.signal.aborted) setError(errorMessage(cause)); }); return () => controller.abort(); }, []);
  useEffect(() => { setChallenge(null); setCode(''); setError(''); setCooldown(0); }, [phone, purpose]);
  useEffect(() => { if (!cooldown) return; const timer = window.setTimeout(() => setCooldown(n => Math.max(0, n - 1)), 1000); return () => clearTimeout(timer); }, [cooldown]);
  async function send() {
    if (!/^1[3-9][0-9]{9}$/.test(phone.trim())) { setError('请先填写有效的 11 位手机号。'); return; }
    const requestedPhone = phone.trim();
    setBusy(true); setError('');
    try { const value = await api<Challenge>('/api/auth/verification-codes', { method: 'POST', body: { phone_number: requestedPhone, purpose } }); if (currentPhone.current !== requestedPhone) return; setChallenge(value); setCode(''); setCooldown(value.resend_after); }
    catch (cause) { setError(errorMessage(cause)); } finally { setBusy(false); }
  }
  return { settings, challenge, code, setCode, busy, error, cooldown, send, valid: !!challenge && /^\d{6}$/.test(code) };
}
export function VerificationFields({ verification: v, disabled = false }: { verification: ReturnType<typeof useVerification>; disabled?: boolean }) {
  return <div className="verification-field"><label className="field-label" htmlFor="verification-code">手机验证码</label><div className="verification-controls"><input id="verification-code" data-testid="otp-code" inputMode="numeric" autoComplete="one-time-code" pattern="[0-9]{6}" maxLength={6} value={v.code} onChange={event => v.setCode(event.target.value.replace(/\D/g, ''))} placeholder="6 位验证码" required disabled={disabled || v.busy} /><button className="button button-secondary" type="button" data-testid="otp-send" disabled={disabled || v.busy || v.cooldown > 0 || !v.settings || v.settings.sms_mode === 'disabled'} onClick={() => void v.send()}>{v.busy ? '正在发送…' : v.cooldown ? `${v.cooldown} 秒后重发` : '获取验证码'}</button></div>{v.settings?.sms_mode === 'disabled' && <Notice>验证码服务尚未启用，请联系运营人员。</Notice>}{v.challenge?.development_code && <Notice>开发演示：不会发送真实短信。本次验证码为 <strong data-testid="development-code">{v.challenge.development_code}</strong>，请在输入框中填写。</Notice>}{v.challenge && !v.challenge.development_code && <p className="field-note">验证码已发送，{Math.floor(v.challenge.expires_in / 60)} 分钟内有效。</p>}{v.error && <p className="feature-error" role="alert">{v.error}</p>}</div>;
}
