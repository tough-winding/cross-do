export interface User {
  user_id: string; user_name: string; user_permission: number; user_status: number;
  real_name_authentication: number; phone_number: string; phone_verified?: number; account_state?: string; real_name?: string | null;
  user_age?: number | null; project_id?: string | null; existing_projects?: number;
  registration_date?: string | null; login_time?: string | null; total_donation_amount?: string | null;
}
export interface ReviewRecord { actor_id: string; decision: string; note: string; time: string; materials?: string[] }
export interface Entry { content: string; attachments: string[]; time?: string; images?: string }
export interface Application {
  account: User; user_id: string; user_status: number; current_volunteer: string | null;
  materials: string[]; verification_record: ReviewRecord[]; previous_volunteers: string[];
  submitted_at?: string; last_change_time?: string;
}
export interface ApplicationTask {
  user_id: string; user_name: string; user_age: number; user_status: number;
  current_volunteer: string | null; submitted_at: string; last_change_time: string;
}
export interface Project {
  project_id: string; user_id?: string; user_name: string; user_age: number; real_name?: string;
  project_status: number; current_volunteer?: string | null; project_create_time: string;
  donation_pool_limit: string | null; current_donations_received: string | null;
  current_used_donations: string | null; last_update_time: string | null;
  pathography: Entry[]; verification_record?: ReviewRecord[]; previous_volunteers?: string[];
}
export type ProjectSummary = Omit<Project, 'pathography' | 'verification_record' | 'previous_volunteers'> & { summary: string };
export type ProjectTask = Project;
export interface Registration { phone_number: string; user_name: string; password: string; user_permission: number; challenge_id: string; code: string }
