import { useRef } from 'react';
import { useAuth } from '../lib/auth';
export type PaymentMode = 'disabled' | 'development' | 'gateway';
export interface Refund { id: string; order_id: string; project_id: string; amount: string; reason: string; status: 'pending' | 'processing' | 'succeeded' | 'failed'; mode: PaymentMode; created_at: string }
export interface Donation { id: string; project_id: string; donor_id: string; amount: string; currency: string; status: 'pending' | 'paid' | 'failed'; refunded_amount: string; refund_reserved: string; mode: PaymentMode; checkout_url?: string | null; created_at: string; refunds?: Refund[] }
export interface Usage { id: string; amount: string; purpose: string; evidence: string[]; completed_at: string }
export interface Funds { project_id: string; balance: string; received: string; used: string; refunded: string; reserved?: string; usage: Usage[] }
export interface Disbursement { id: string; project_id: string; requester_id: string; amount: string; purpose: string; evidence: string[]; payout_reference?: string; status: string; mode: PaymentMode; review_note?: string; created_at: string; updated_at: string }
export interface Closure { id?: string; project_id: string; reason: string; remaining_plan: string; status: string; note?: string; decision_note?: string; review_note?: string; requested_at?: string; reviewed_at?: string; closed_at?: string }
export const financeStatus: Record<string, string> = { pending: '待处理', paid: '已支付', failed: '处理失败', processing: '处理中', succeeded: '已完成', requested: '等待审核', approved: '已通过审核', rejected: '未通过审核' };
export function cents(value?: string | null): bigint {
  const match = /^(\d+)(?:\.(\d{1,2}))?$/.exec(value || '0');
  if (!match) throw new Error('金额格式不正确。');
  return BigInt(match[1]) * 100n + BigInt((match[2] || '').padEnd(2, '0'));
}
export function decimal(value: bigint): string { return `${value / 100n}.${String(value % 100n).padStart(2, '0')}`; }
export function positiveAmount(value: string): string {
  if (!/^(?:0|[1-9][0-9]{0,7})(?:\.[0-9]{1,2})?$/.test(value.trim()) || cents(value.trim()) <= 0n) throw new Error('请输入大于 0 的金额，最多两位小数。');
  return decimal(cents(value.trim()));
}
export function refundAvailable(order: Donation): bigint {
  const value = cents(order.amount) - cents(order.refunded_amount) - cents(order.refund_reserved);
  return value > 0n ? value : 0n;
}
export function checkoutLink(value?: string | null): string | null {
  try { const url = new URL(value || ''); return url.protocol === 'https:' && !url.username && !url.password ? url.toString() : null; } catch { return null; }
}
export function useRequestKey(scope: string) {
  const { user } = useAuth();
  const fallback = useRef<Record<string, Record<string, string>>>({});
  const active = useRef<{ storage: string; fingerprint: string } | null>(null);
  function read(storage: string): Record<string, string> {
    try {
      const stored: unknown = JSON.parse(sessionStorage.getItem(storage) || '{}');
      if (stored && typeof stored === 'object' && !Array.isArray(stored)) {
        return { ...(fallback.current[storage] || {}), ...Object.fromEntries(Object.entries(stored).filter(([hash, key]) => /^[0-9a-f]{64}$/.test(hash) && typeof key === 'string' && /^[0-9a-f-]{36}$/i.test(key))) };
      }
    } catch { /* Keep retries safe when browser storage is unavailable. */ }
    return fallback.current[storage] || {};
  }
  function write(storage: string, values: Record<string, string>) {
    fallback.current[storage] = values;
    try { if (Object.keys(values).length) sessionStorage.setItem(storage, JSON.stringify(values)); else sessionStorage.removeItem(storage); } catch { /* In-memory retry protection still applies. */ }
  }
  return {
    keyFor: async (payload: unknown): Promise<string> => {
      const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(JSON.stringify(payload)));
      const fingerprint = Array.from(new Uint8Array(digest), byte => byte.toString(16).padStart(2, '0')).join('');
      const storage = `crossdo.pending:${user?.user_id || 'anonymous'}:${scope}`;
      const values = read(storage);
      values[fingerprint] ||= crypto.randomUUID();
      write(storage, values); active.current = { storage, fingerprint };
      return values[fingerprint];
    },
    reset: () => {
      if (!active.current) return;
      const { storage, fingerprint } = active.current;
      const values = read(storage); delete values[fingerprint]; write(storage, values); active.current = null;
    },
  };
}
