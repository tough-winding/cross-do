import { useCallback, useEffect, useState, type FormEvent } from 'react';
import { ArrowRight, Check, CheckCheck, Clock3, FileCheck2, FileText, Info, Leaf, Link2, LoaderCircle, LockKeyhole, Plus, RefreshCw, ShieldCheck, Trash2 } from 'lucide-react';
import { Link } from 'react-router-dom';
import { useAuth } from '../lib/auth';
import { api, ApiError, errorMessage } from '../lib/api';
import { formatDate, safeLinks } from '../lib/format';
import type { Application, ReviewRecord } from '../lib/types';
import './forms.css';
import UploadField from '../components/UploadField';
import { MaterialLinks } from '../components/ui';

const statusContent: Record<number, { title: string; description: string }> = {
  0: { title: '资料已通过核验', description: '感谢你的耐心。现在可以创建求助项目，写下病情、治疗计划和需要的帮助。' },
  1: { title: '让我们先了解你的情况', description: '完整填写身份信息与证明材料，让志愿者能够更快了解并核验你的求助。' },
  2: { title: '资料已提交，等待志愿者接单', description: '志愿者接单后会逐项核验资料，并根据你填写的手机号与你联系。' },
  3: { title: '志愿者正在核验资料', description: '你的资料已有志愿者接单。请留意电话，并耐心等待审核结果。' },
  4: { title: '志愿者暂时未能联系到你', description: '请确认账号中的手机号可以联系，查看审核留言并补充资料后重新提交。' },
  5: { title: '再补充一些资料，就能继续向前', description: '请查看下方志愿者的审核留言，补齐材料后重新提交。之前的审核记录会保留。' },
  6: { title: '补充资料已提交，等待再次审核', description: '我们已保留你的新资料和过往审核记录，等待志愿者接单。' },
  7: { title: '志愿者正在复核补充资料', description: '资料正在重新核验中，请保持电话畅通。审核结果会显示在这里。' },
};

const decisionNames: Record<string, string> = {
  submit: '患者提交资料', claim: '志愿者领取审核', approve: '资料审核通过', correction: '需要补充资料', unreachable: '暂时无法取得联系',
};

function History({ records }: { records: ReviewRecord[] }) {
  if (!records.length) return <p className="field-note">提交资料后，审核进展会记录在这里。</p>;
  return <ol className="application-history-list">{[...records].reverse().map((record, index) => {
    const systemNote = record.decision === 'submit' ? '求助资料已提交，等待志愿者核验。' : record.decision === 'claim' ? '志愿者已领取本次审核任务。' : record.note;
    return <li className="application-history-item" key={`${record.time}-${record.decision}-${index}`}>
      <div className="application-history-title"><strong>{decisionNames[record.decision] || '资料进展'}</strong><time>{formatDate(record.time)}</time></div>
      <p>{systemNote}</p>
    </li>;
  })}</ol>;
}

export default function ApplicationPage() {
  const { user, refreshUser } = useAuth();
  const [application, setApplication] = useState<Application | null>(null);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');
  const [realName, setRealName] = useState(user?.real_name || '');
  const [idCard, setIdCard] = useState('');
  const [age, setAge] = useState(user?.user_age == null ? '' : String(user.user_age));
  const [materials, setMaterials] = useState<string[]>(['']);
  const userId = user?.user_id;

  const load = useCallback(async (signal?: AbortSignal) => {
    if (!userId) { setLoading(false); return; }
    setLoading(true);
    setError('');
    try {
      const result = await api<Application>(`/api/applications/${encodeURIComponent(userId)}`, { signal });
      setApplication(result);
      setRealName(result.account.real_name || '');
      setAge(result.account.user_age == null ? '' : String(result.account.user_age));
      setMaterials(result.materials?.length ? result.materials : ['']);
      // Keep certification checks elsewhere in sync with this freshly loaded review.
      await refreshUser().catch(() => null);
    } catch (cause) {
      if (signal?.aborted) return;
      if (cause instanceof ApiError && cause.status === 404) setApplication(null);
      else setError(errorMessage(cause, '暂时无法获取资料进度，请稍后重试。'));
    } finally {
      if (!signal?.aborted) setLoading(false);
    }
  }, [userId, refreshUser]);

  useEffect(() => {
    const controller = new AbortController();
    void load(controller.signal);
    return () => controller.abort();
  }, [load]);

  if (!user) return null;
  if (user.user_permission !== 3) return <div className="page application-page">
    <div className="application-heading"><div><span className="form-eyebrow">YOUR JOURNEY</span><h1>一起，成为温暖的力量</h1><p>求助资料申请仅面向患者账号。</p></div></div>
    <div className="application-empty"><ShieldCheck size={35} /><h2>{[5, 7].includes(user.user_permission) ? '志愿者身份由平台完成核验' : '你的每一份关注，都是一份支持'}</h2><p>{[5, 7].includes(user.user_permission) ? '志愿者完成平台核验后，即可在任务中心领取资料和项目审核任务。请在账号页面查看当前身份状态。' : '你可以在求助项目中了解已审核通过的患者故事，陪伴他们继续向前。'}</p><Link to={[5, 7].includes(user.user_permission) ? '/account' : '/projects'} className="auth-submit">{[5, 7].includes(user.user_permission) ? '查看我的账号' : '浏览求助项目'}<ArrowRight size={15} /></Link></div>
  </div>;

  const account = application?.account || user;
  const status = application?.user_status ?? user.user_status;
  const content = statusContent[status];
  const editable = [1, 4, 5].includes(status);
  const verified = status === 0;
  const step = verified ? 2 : [2, 3, 6, 7].includes(status) ? 1 : 0;
  const availableLinks = safeLinks(application?.materials || []);

  async function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setError(''); setSuccess('');
    const links = materials.map(value => value.trim());
    if (!links.length || links.some(value => !safeLinks([value]).length)) {
      setError('请上传证明材料或填写有效的 HTTP(S) 文件链接。');
      return;
    }
    const patientAge = Number(age);
    if (age === '' || !Number.isInteger(patientAge) || patientAge < 0 || patientAge > 120) {
      setError('请填写 0–120 岁之间的完整年龄。');
      return;
    }
    setBusy(true);
    try {
      const result = await api<Application>('/api/me/application', { method: 'POST', body: {
        real_name: realName.trim(), id_card: idCard.trim().toUpperCase(), user_age: patientAge, materials: links,
      } });
      setApplication(result);
      setIdCard('');
      setSuccess('资料已提交。请保持电话畅通，你可以随时回到这里查看审核进展。');
      await refreshUser().catch(() => null);
    } catch (cause) {
      setError(errorMessage(cause, '资料暂时未能提交，请检查后重试。'));
    } finally { setBusy(false); }
  }

  return <div className="page application-page">
    <header className="application-heading">
      <div><span className="form-eyebrow">EVERY JOURNEY BEGINS WITH TRUST</span><h1>我的求助资料</h1><p>让真实被看见，让需要被理解。</p></div>
      <button className="application-heading-action" type="button" aria-label="刷新审核进度" onClick={() => { void load(); }} disabled={loading || busy}><RefreshCw size={14} className={loading ? 'spin' : ''} /><span>刷新进度</span></button>
    </header>
    <div className="application-steps" aria-label="资料核验流程">{['填写求助资料', '志愿者核验', '认证完成'].map((label, index) => <div key={label} className={`application-step ${step > index ? 'done' : step === index ? 'current' : ''}`} aria-current={step === index ? 'step' : undefined}><span className="application-step-number">{step > index ? <Check size={16} /> : `0${index + 1}`}</span><span className="application-step-name">{label}</span></div>)}</div>
    {loading ? <div className="application-loading"><LoaderCircle size={19} className="spin" />正在查看你的资料进度…</div> : <>
      {error && <div className="form-error" role="alert"><Info size={16} /><span>{error}<button type="button" onClick={() => void load()}>重新获取进度</button></span></div>}
      {success && <div className="form-success" role="status"><CheckCheck size={17} /><span>{success}</span></div>}
      <div className="application-layout">
        <div>
          {content && (status !== 1) && <section className={`application-status-panel ${[4, 5].includes(status) ? 'correction' : ''}`}>
            {verified ? <ShieldCheck size={25} /> : [4, 5].includes(status) ? <FileText size={25} /> : <Clock3 size={25} />}
            <div><h2>{content.title}</h2><p>{content.description}</p>{verified && <Link className="auth-submit" to="/my-project">{user.project_id ? '查看我的求助项目' : '创建我的求助项目'}<ArrowRight size={15} /></Link>}</div>
          </section>}
          <section className="application-card">
            <div className="application-card-head"><span className="application-card-icon"><FileCheck2 size={20} /></span><div><h2>{editable ? [4, 5].includes(status) ? '补充并重新提交资料' : '填写你的求助资料' : '已提交的资料'}</h2><p>{editable ? '请填写真实信息，帮助志愿者了解你的情况' : '完整身份证号码不会在页面中显示'}</p></div></div>
            <div className="application-card-body">
              {editable ? <form className="patient-form" data-testid="application-form" onSubmit={submit} aria-busy={busy}>
                <div className="form-grid-two">
                  <div className="form-field"><label htmlFor="application-real-name">真实姓名</label><input id="application-real-name" data-testid="application-real-name" autoComplete="off" maxLength={20} placeholder="与身份证件一致" value={realName} onChange={event => setRealName(event.target.value)} required disabled={busy} /></div>
                  <div className="form-field"><label htmlFor="application-age">年龄</label><input id="application-age" data-testid="application-age" type="number" inputMode="numeric" min={0} max={120} step={1} placeholder="请输入周岁年龄" value={age} onChange={event => setAge(event.target.value)} required disabled={busy} /></div>
                </div>
                <div className="form-field"><label htmlFor="application-id-card">身份证号码</label><input id="application-id-card" data-testid="application-id-card" autoComplete="off" maxLength={18} pattern="[0-9]{17}[0-9Xx]" placeholder="请输入 18 位身份证号码" value={idCard} onChange={event => setIdCard(event.target.value)} required disabled={busy} aria-describedby="identity-hint" /><p className="field-note" id="identity-hint">{[4, 5].includes(status) ? '为保护隐私，原身份证号码不会回显。重新提交时，请再次填写。' : '用于资料核验，提交后不会在页面中完整显示。'}</p></div>
                <hr className="form-divider" />
                <div className="form-field"><div className="form-label-line"><label className="field-label" htmlFor="application-material-0">证明材料</label><span className="field-note">{materials.length} / 20</span></div><p className="field-note">可提供病历、诊断证明、身份与治疗费用等材料的文件链接。</p>
                  <UploadField purpose="application" count={materials.filter(Boolean).length} disabled={busy} onBusyChange={setUploading} onUploaded={url => setMaterials(current => [...current.filter(value => value.trim()), url])} /><div className="link-field-list">{materials.map((value, index) => <div className="link-field-row" key={index}><span className="link-field-number" aria-hidden="true">{String(index + 1).padStart(2, '0')}</span><input id={`application-material-${index}`} data-testid={`application-material-${index}`} type="text" inputMode="url" aria-label={`第 ${index + 1} 份证明材料的已上传文件链接`} maxLength={2048} placeholder="https://…" value={value} onChange={event => setMaterials(current => current.map((item, i) => i === index ? event.target.value : item))} required disabled={busy} />{materials.length > 1 && <button className="link-field-remove" type="button" aria-label={`删除第 ${index + 1} 份材料`} onClick={() => setMaterials(current => current.filter((_, i) => i !== index))} disabled={busy}><Trash2 size={14} /></button>}</div>)}</div>
                  <button className="link-field-add" type="button" onClick={() => setMaterials(current => [...current, ''])} disabled={busy || materials.length >= 20}><Plus size={14} />添加一份材料</button>
                  <div className="application-material-hint"><Link2 size={17} /><span>可以上传本地文件，也可以补充已有文件的 HTTP(S) 链接。外部链接的访问权限由原文件服务控制。</span></div>
                </div>
                <div className="form-privacy"><LockKeyhole size={14} /><span>身份与医疗材料仅对你和当前审核志愿者开放。请确认材料清晰、完整，并保持手机号 {user.phone_number} 可以联系。</span></div>
                <button className="auth-submit" data-testid="application-submit" type="submit" disabled={busy || uploading}>{busy ? <><LoaderCircle size={16} className="spin" />正在提交…</> : <>{[4, 5].includes(status) ? '提交补充资料' : '提交资料，等待核验'}<ArrowRight size={16} /></>}</button>
              </form> : <>
                <dl className="application-read-grid"><div><dt>真实姓名</dt><dd>{account.real_name || '尚未填写'}</dd></div><div><dt>年龄</dt><dd>{account.user_age == null ? '尚未填写' : `${account.user_age} 岁`}</dd></div><div><dt>联系手机号</dt><dd>{account.phone_number || '暂无'}</dd></div><div><dt>最近提交</dt><dd>{formatDate(application?.submitted_at)}</dd></div></dl>
                <h3 className="field-label">证明材料</h3>
                <MaterialLinks links={availableLinks} label="证明材料" />
                {!content && <p className="form-error" role="status">当前账号暂不能提交新的求助资料，请在账号页面查看状态。</p>}
              </>}
            </div>
          </section>
          <section className="application-card application-history"><div className="application-card-body"><h2 className="application-history-heading">每一步进展，都在这里</h2><History records={application?.verification_record || []} /></div></section>
        </div>
        <aside className="application-sidebar" aria-label="资料填写提示">
          <div className="application-side-card"><Leaf size={27} /><h3>认真准备每一份资料，<br />让善意更快抵达。</h3><p>清晰、完整的证明材料，能帮助志愿者准确了解你的情况。请尽量提供近期病历、诊断及治疗费用依据。</p><div className="application-side-rule"><ShieldCheck size={15} /><span>审核不是一道门槛，而是让每一份帮助建立在信任之上。</span></div></div>
          <div className="application-side-help"><h3>接下来会发生什么？</h3><p>提交后，资料会进入志愿者任务池。接单志愿者将核验材料并与你联系；如需补充，你可以在本页查看留言并重新提交。</p></div>
        </aside>
      </div>
    </>}
  </div>;
}
