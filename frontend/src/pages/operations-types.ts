import type { User } from '../lib/types';

export interface OperationsUser extends User { account_state: 'active' | 'frozen'; phone_verified?: number }
export interface AdminOverview { pending_volunteers: number; pending_applications: number; pending_projects: number; open_cases: number; pending_closures: number }
export interface AdminTask { kind: 'application' | 'project'; id: string; user_id: string; user_name: string; status: number; current_volunteer: string | null; updated_at: string; overdue: number }
export interface NotificationItem { id: string; type: string; title: string; body: string; link: string | null; created_at: string; read_at: string | null }
export interface CaseEvent { actor_id: string; action: string; note: string; created_at: string }
export interface CaseReport {
  id: string; reporter_id: string; target_user_id: string | null; project_id: string | null; parent_id: string | null;
  kind: 'report' | 'appeal'; reason: string; evidence: string[]; status: 'open' | 'resolved' | 'dismissed';
  decision_note: string | null; created_at: string; updated_at: string; resolved_at: string | null; events?: CaseEvent[]; evidence_private?: boolean;
}
export interface ProjectClosure { project_id: string; requester_id: string; user_name?: string; reason: string; remaining_plan: 'refund' | 'fully_used'; status: 'pending' | 'approved' | 'rejected'; decision_note: string | null; requested_at: string; reviewed_at: string | null }
export interface RefundItem { id: string; order_id: string; project_id: string; amount: string; status: string; reason?: string; mode?: string; created_at?: string }
export interface FinanceOrder { id: string; project_id: string; donor_id: string; donor_name?: string; amount: string; refunded_amount: string; refund_reserved?: string; status: string; mode: string; created_at?: string; refunds?: RefundItem[] }
export interface Disbursement { id: string; project_id: string; user_id?: string; amount: string; purpose: string; evidence: string[]; payout_reference?: string; status: string; mode?: string; created_at?: string; review_note?: string }
export interface Reconciliation { project_id: string; balance: string; cache_balance: string; ledger_balance: string; matches: boolean | number; reserved?: string; received?: string; used?: string; refunded?: string }
