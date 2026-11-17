import { Link } from 'react-router-dom';
import { ArrowUpRight, ShieldCheck, UserRound } from 'lucide-react';
import { useAuth } from '../lib/auth';
import { formatDate, roleLabel } from '../lib/format';
import PhoneChange from '../components/PhoneChange';
import { Notice, PageHeading, StatusBadge } from '../components/ui';
export default function AccountPage() {
  const { user } = useAuth();
  if (!user) return null;
  return <div className="page account-page"><PageHeading eyebrow="YOUR CROSSDO" title="每一份参与，都有自己的位置" description="查看您的账户资料与认证进展。" /><section className="panel account-card"><div className="account-profile"><span className="account-avatar"><UserRound size={32} strokeWidth={1.3} /></span><div><h2>{user.user_name}</h2><p>{roleLabel(user.user_permission)}</p></div><StatusBadge status={user.user_status} /></div><dl className="account-details"><div><dt>注册手机号</dt><dd>{user.phone_number}</dd></div><div><dt>认证情况</dt><dd><ShieldCheck size={15} />{user.real_name_authentication === 1 ? '资料认证已通过' : '尚未完成资料认证'}</dd></div><div><dt>加入渡舟</dt><dd>{formatDate(user.registration_date)}</dd></div><div><dt>最近登录</dt><dd>{formatDate(user.login_time)}</dd></div></dl>{user.user_permission === 3 && <Link to="/application" className="button button-secondary">查看申请进展 <ArrowUpRight size={16} /></Link>}{[5, 7].includes(user.user_permission) && user.real_name_authentication !== 1 && <Notice>志愿者需完成线下身份与资格核验。通过后，您就可以在审核工作台领取任务。</Notice>}</section><PhoneChange /></div>;
}
