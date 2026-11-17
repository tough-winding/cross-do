import { useEffect, useState, type FormEvent } from 'react';
import { ArrowLeft, ArrowRight, Clock3, Eye, EyeOff, Heart, Leaf, LoaderCircle, LockKeyhole, RefreshCw, ShieldCheck, Waves } from 'lucide-react';
import { Link, Navigate, useLocation, useNavigate, useSearchParams } from 'react-router-dom';
import { useAuth } from '../lib/auth';
import { errorMessage } from '../lib/api';
import './forms.css';
import { useVerification, VerificationFields } from '../features/verification';

const roles = [
  { id: 2, label: '我想帮助', icon: Heart },
  { id: 3, label: '我需要帮助', icon: Leaf },
  { id: 5, label: '成为志愿者', icon: ShieldCheck },
];

export default function AuthPage() {
  const location = useLocation();
  const navigate = useNavigate();
  const [searchParams] = useSearchParams();
  const registering = location.pathname === '/register';
  const { user, token, loading, expired, restorationError, retrySession, login, register, logout } = useAuth();
  const [phone, setPhone] = useState('');
  const [password, setPassword] = useState('');
  const verification = useVerification(phone, 'register');
  const [name, setName] = useState('');
  const [role, setRole] = useState(() => {
    const requested = Number(searchParams.get('role'));
    return [2, 3, 5].includes(requested) ? requested : 2;
  });
  const [showPassword, setShowPassword] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [phoneChanged] = useState(() => sessionStorage.getItem('crossdo.phone-changed') === '1');
  const next = searchParams.get('next');
  const safeNext = next?.startsWith('/') && !next.startsWith('//') && !/^\/(login|register)(?:[/?]|$)/.test(next)
    ? next : null;
  const modeSuffix = safeNext ? `?next=${encodeURIComponent(safeNext)}` : '';

  useEffect(() => { setError(''); }, [registering]);
  useEffect(() => { if (phoneChanged) sessionStorage.removeItem('crossdo.phone-changed'); }, [phoneChanged]);

  if (loading) return <div className="application-loading"><LoaderCircle className="spin" size={20} />正在恢复登录状态…</div>;
  if (restorationError && token) return <div className="page auth-recovery-page">
    <section className="application-empty" aria-labelledby="session-recovery-title">
      <RefreshCw size={34} />
      <h2 id="session-recovery-title">登录状态暂未恢复</h2>
      <p role="status">{restorationError}</p>
      <p>你的登录信息仍在，请重试连接，无需重新注册。</p>
      <div className="auth-recovery-actions">
        <button type="button" className="auth-submit" onClick={() => void retrySession()}><RefreshCw size={15} />重新连接</button>
        <button type="button" className="auth-recovery-logout" onClick={() => { void logout().catch(() => null); }}>退出当前登录</button>
      </div>
    </section>
  </div>;
  if (user && !busy) return <Navigate to={safeNext || '/'} replace />;

  async function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setError('');
    if (new TextEncoder().encode(password).length > 72) {
      setError('密码不能超过 72 个 UTF-8 字节，请缩短后重试。');
      return;
    }
    setBusy(true);
    try {
      if (registering) {
        if (!verification.valid || !verification.challenge) throw new Error('请获取手机验证码并填写 6 位验证码。');
        await register({ phone_number: phone.trim(), user_name: name.trim(), password, user_permission: role, challenge_id: verification.challenge.challenge_id, code: verification.code });
      } else {
        await login(phone.trim(), password);
      }
      setPassword('');
      navigate(safeNext || (registering && role === 3 ? '/application' : registering && role === 5 ? '/account' : '/'), { replace: true });
    } catch (cause) {
      setError(errorMessage(cause, registering ? '暂时无法创建账号，请稍后重试。' : '登录失败，请检查手机号和密码。'));
    } finally {
      setBusy(false);
    }
  }

  return <div className="page auth-page">
    <aside className="auth-story" aria-label="渡舟的理念">
      <div>
        <div className="auth-story-brand"><span className="auth-story-mark"><Waves size={23} /></span>渡舟 · CROSSDO</div>
        <h1>每一份微光，<br />都能照亮一段归途。</h1>
        <p className="auth-story-intro">我们相信，困境中的每一个人都值得被看见。让真实的求助与温暖的善意，在这里相遇。</p>
        <div className="auth-story-rule"><ShieldCheck size={17} />志愿者逐项核验求助资料</div>
        <div className="auth-story-rule"><LockKeyhole size={17} />个人资料仅向审核人员开放</div>
        <div className="auth-story-rule"><Heart size={17} />持续记录每一步治疗与希望</div>
      </div>
      <div className="auth-story-bottom"><span>愿予清风，助渡舟。</span>从一次看见，到一程陪伴。<br />每一份善意，都有抵达的方向。</div>
    </aside>
    <section className="auth-form-side" aria-labelledby="auth-title">
      <div className="auth-form-box">
        <span className="auth-kicker">A LITTLE KINDNESS, A NEW BEGINNING</span>
        <h2 id="auth-title">{registering ? '让我们，从这里相遇' : '欢迎回到渡舟'}</h2>
        <p className="auth-form-intro">{registering ? '创建账号，让需要与善意找到彼此。' : '登录账号，继续你的温暖之旅。'}</p>
        <div className="auth-mode-switch" aria-label="账号操作">
          <Link to={`/login${modeSuffix}`} className={!registering ? 'active' : ''}>手机号登录</Link>
          <Link to={`/register${modeSuffix}`} className={registering ? 'active' : ''}>注册账号</Link>
        </div>
        {(phoneChanged || searchParams.get("phone_changed") === "1") && !registering && <div className="form-success" role="status">手机号已更新，请使用新手机号和原密码重新登录。</div>}
        {expired && !registering && <div className="auth-session-notice" role="status"><Clock3 size={16} /><span>登录已过期，请重新登录后继续。你的求助资料和项目进度会保留。</span></div>}
        <form className="auth-form" data-testid="auth-form" onSubmit={submit} aria-busy={busy}>
          {registering && <fieldset className="auth-role-fieldset">
            <legend className="field-label">你希望如何参与</legend>
            <div className="auth-roles">
              {roles.map(({ id, label, icon: Icon }) => <button key={id} type="button" data-testid={`role-${id}`} className={`auth-role ${role === id ? 'active' : ''}`} aria-pressed={role === id} disabled={busy} onClick={() => setRole(id)}><Icon size={20} /><span>{label}</span></button>)}
            </div>
          </fieldset>}
          {registering && <div className="form-field">
            <label htmlFor="auth-name">怎么称呼你</label>
            <input id="auth-name" data-testid="name-input" name="user_name" autoComplete="nickname" placeholder="填写一个温暖的名字" value={name} onChange={event => setName(event.target.value)} maxLength={24} required disabled={busy} />
          </div>}
          <div className="form-field">
            <label htmlFor="auth-phone">手机号</label>
            <input id="auth-phone" data-testid="phone-input" name="phone_number" type="tel" inputMode="numeric" autoComplete="username" pattern="1[3-9][0-9]{9}" maxLength={11} placeholder="请输入 11 位手机号" value={phone} onChange={event => setPhone(event.target.value)} required disabled={busy || verification.busy} />
          </div>
          {registering && <VerificationFields verification={verification} disabled={busy} />}
          <div className="form-field">
            <label htmlFor="auth-password">{registering ? '设置登录密码' : '登录密码'}</label>
            <div className="password-input">
              <input id="auth-password" data-testid="password-input" name="password" type={showPassword ? 'text' : 'password'} autoComplete={registering ? 'new-password' : 'current-password'} minLength={8} maxLength={72} placeholder={registering ? '至少 8 位，建议包含字母和数字' : '请输入你的登录密码'} value={password} onChange={event => setPassword(event.target.value)} required disabled={busy} aria-describedby={registering ? 'auth-password-hint' : undefined} />
              <button className="password-toggle" type="button" onClick={() => setShowPassword(!showPassword)} aria-label={showPassword ? '隐藏密码' : '显示密码'} aria-pressed={showPassword}>{showPassword ? <EyeOff size={17} /> : <Eye size={17} />}</button>
            </div>
            {registering && <p className="field-note" id="auth-password-hint">使用 8–72 位密码；中文等字符按 UTF-8 字节计算长度。</p>}
          </div>
          {error && <div className="form-error" role="alert"><ShieldCheck size={16} /><span>{error}{registering && error.includes('账号已创建') && <Link className="auth-error-link" to={`/login${modeSuffix}`}>前往手机号登录<ArrowRight size={13} /></Link>}</span></div>}
          <button className="auth-submit" data-testid="auth-submit" type="submit" disabled={busy}>{busy ? <><LoaderCircle className="spin" size={17} />{registering ? '正在创建账号…' : '正在登录…'}</> : <>{registering ? '注册并开启旅程' : '登录渡舟'}<ArrowRight size={17} /></>}</button>
        </form>
        {!registering && <Link className="feature-subtle-link" to="/recover">忘记密码？通过手机号找回</Link>}
        <p className="auth-fineprint">{registering ? '患者注册后需要提交资料，由志愿者完成核验。' : '使用注册时设置的手机号和密码登录。'}<br />{registering && role === 5 ? '志愿者账号须经平台核验后，才可领取审核任务。' : '愿每一次相遇，都成为一份温暖的开始。'}</p>
        <Link className="auth-home" to="/"><ArrowLeft size={14} />返回渡舟首页</Link>
      </div>
    </section>
  </div>;
}
