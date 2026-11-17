-- Apply after 002, with writers stopped. No existing review state is overwritten.
SET NAMES utf8mb4;
DROP PROCEDURE IF EXISTS crossdo_operations_columns;
DELIMITER $$
CREATE PROCEDURE crossdo_operations_columns()
BEGIN
  IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema=DATABASE() AND table_name='user' AND column_name='account_state') THEN
    ALTER TABLE `user` ADD COLUMN account_state VARCHAR(16) NOT NULL DEFAULT 'active',
      ADD CONSTRAINT ck_user_account_state CHECK (account_state IN ('active','frozen'));
  END IF;
  IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema=DATABASE() AND table_name='user' AND column_name='session_version') THEN
    ALTER TABLE `user` ADD COLUMN session_version BIGINT UNSIGNED NOT NULL DEFAULT 0;
  END IF;
END$$
DELIMITER ;
CALL crossdo_operations_columns();
DROP PROCEDURE crossdo_operations_columns;

CREATE TABLE IF NOT EXISTS notification (
  id CHAR(36) NOT NULL PRIMARY KEY,
  user_id CHAR(36) NOT NULL,
  type VARCHAR(32) NOT NULL,
  title VARCHAR(120) NOT NULL,
  body TEXT NOT NULL,
  link VARCHAR(200) NOT NULL,
  created_at DATETIME NOT NULL,
  read_at DATETIME DEFAULT NULL,
  KEY idx_notification_user_time(user_id,created_at,id),
  KEY idx_notification_unread(user_id,read_at),
  CONSTRAINT fk_notification_user FOREIGN KEY(user_id) REFERENCES `user`(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS admin_audit (
  id CHAR(36) NOT NULL PRIMARY KEY,
  actor_id CHAR(36) NOT NULL,
  action VARCHAR(40) NOT NULL,
  target_type VARCHAR(24) NOT NULL,
  target_id CHAR(36) NOT NULL,
  note TEXT NOT NULL,
  created_at DATETIME NOT NULL,
  KEY idx_admin_audit_target(target_type,target_id,created_at,id),
  KEY idx_admin_audit_actor(actor_id,created_at,id),
  CONSTRAINT fk_admin_audit_actor FOREIGN KEY(actor_id) REFERENCES `user`(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS case_report (
  id CHAR(36) NOT NULL PRIMARY KEY,
  reporter_id CHAR(36) NOT NULL,
  target_user_id CHAR(36) DEFAULT NULL,
  project_id CHAR(36) DEFAULT NULL,
  parent_id CHAR(36) DEFAULT NULL,
  kind VARCHAR(16) NOT NULL,
  reason TEXT NOT NULL,
  evidence JSON NOT NULL,
  status VARCHAR(16) NOT NULL DEFAULT 'open',
  decision_note TEXT DEFAULT NULL,
  resolved_by CHAR(36) DEFAULT NULL,
  created_at DATETIME NOT NULL,
  updated_at DATETIME NOT NULL,
  resolved_at DATETIME DEFAULT NULL,
  KEY idx_case_reporter(reporter_id,created_at,id),
  KEY idx_case_target(target_user_id,created_at,id),
  KEY idx_case_project(project_id,created_at,id),
  KEY idx_case_queue(status,created_at,id),
  CONSTRAINT ck_case_target CHECK ((target_user_id IS NOT NULL) <> (project_id IS NOT NULL)),
  CONSTRAINT ck_case_kind CHECK (kind IN ('report','appeal')),
  CONSTRAINT ck_case_status CHECK (status IN ('open','resolved','dismissed')),
  CONSTRAINT ck_case_evidence CHECK (JSON_TYPE(evidence)='ARRAY'),
  CONSTRAINT fk_case_reporter FOREIGN KEY(reporter_id) REFERENCES `user`(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_case_target FOREIGN KEY(target_user_id) REFERENCES `user`(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_case_project FOREIGN KEY(project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_case_parent FOREIGN KEY(parent_id) REFERENCES case_report(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_case_resolver FOREIGN KEY(resolved_by) REFERENCES `user`(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS case_event (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  case_id CHAR(36) NOT NULL,
  actor_id CHAR(36) NOT NULL,
  action VARCHAR(24) NOT NULL,
  note TEXT NOT NULL,
  created_at DATETIME NOT NULL,
  KEY idx_case_event_case(case_id,id),
  CONSTRAINT fk_case_event_case FOREIGN KEY(case_id) REFERENCES case_report(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_case_event_actor FOREIGN KEY(actor_id) REFERENCES `user`(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS project_closure (
  project_id CHAR(36) NOT NULL PRIMARY KEY,
  requester_id CHAR(36) NOT NULL,
  reason TEXT NOT NULL,
  remaining_plan VARCHAR(16) NOT NULL,
  status VARCHAR(16) NOT NULL DEFAULT 'pending',
  decision_note TEXT DEFAULT NULL,
  reviewed_by CHAR(36) DEFAULT NULL,
  requested_at DATETIME NOT NULL,
  reviewed_at DATETIME DEFAULT NULL,
  KEY idx_closure_queue(status,requested_at,project_id),
  CONSTRAINT ck_closure_plan CHECK (remaining_plan IN ('refund','fully_used')),
  CONSTRAINT ck_closure_status CHECK (status IN ('pending','approved','rejected')),
  CONSTRAINT fk_closure_project FOREIGN KEY(project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_closure_requester FOREIGN KEY(requester_id) REFERENCES `user`(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_closure_reviewer FOREIGN KEY(reviewed_by) REFERENCES `user`(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

INSERT INTO schema_migration(version,applied_at) VALUES('003_operations',UTC_TIMESTAMP())
ON DUPLICATE KEY UPDATE version=VALUES(version);
