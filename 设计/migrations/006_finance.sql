-- Payment adapters use explicit development/gateway modes. Never mix them within a project.
-- Run after migrations 003..005, with writers stopped. Re-runnable; no historical money is fabricated.
SET NAMES utf8mb4;
ALTER TABLE project MODIFY current_donations_received DECIMAL(14,2) NOT NULL DEFAULT 0.00,
                    MODIFY current_used_donations DECIMAL(14,2) NOT NULL DEFAULT 0.00;
ALTER TABLE user MODIFY total_donation_amount DECIMAL(14,2) DEFAULT NULL;
CREATE TABLE IF NOT EXISTS project_fund_account (
 project_id CHAR(36) NOT NULL PRIMARY KEY,
 mode VARCHAR(16) NOT NULL,
 balance DECIMAL(14,2) NOT NULL DEFAULT 0.00,
 reserved DECIMAL(14,2) NOT NULL DEFAULT 0.00,
 updated_at DATETIME NOT NULL,
 CONSTRAINT ck_fund_account_mode CHECK(mode IN ('development','gateway')),
 CONSTRAINT ck_fund_account_amounts CHECK(balance>=0 AND reserved>=0 AND reserved<=balance),
 CONSTRAINT fk_fund_account_project FOREIGN KEY(project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
CREATE TABLE IF NOT EXISTS payment_order (
 id CHAR(36) NOT NULL PRIMARY KEY, project_id CHAR(36) NOT NULL, donor_id CHAR(36) NOT NULL,
 amount DECIMAL(10,2) NOT NULL, currency CHAR(3) NOT NULL DEFAULT 'CNY',
 status VARCHAR(16) NOT NULL DEFAULT 'pending', mode VARCHAR(16) NOT NULL,
 refunded_amount DECIMAL(10,2) NOT NULL DEFAULT 0.00, refund_reserved DECIMAL(10,2) NOT NULL DEFAULT 0.00,
 idempotency_key CHAR(36) NOT NULL, checkout_url VARCHAR(2048) DEFAULT NULL,
 created_at DATETIME NOT NULL, updated_at DATETIME NOT NULL, paid_at DATETIME DEFAULT NULL,
 UNIQUE KEY uq_payment_idempotency(donor_id,idempotency_key),
 KEY idx_payment_donor(donor_id,created_at,id), KEY idx_payment_project(project_id,status,created_at,id),
 CONSTRAINT ck_payment_amount CHECK(amount>0 AND refunded_amount>=0 AND refund_reserved>=0 AND refunded_amount+refund_reserved<=amount),
 CONSTRAINT ck_payment_status CHECK(status IN ('pending','paid','failed')),
 CONSTRAINT ck_payment_mode CHECK(mode IN ('development','gateway')),
 CONSTRAINT ck_payment_currency CHECK(currency='CNY'),
 CONSTRAINT fk_payment_project FOREIGN KEY(project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
 CONSTRAINT fk_payment_donor FOREIGN KEY(donor_id) REFERENCES user(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
CREATE TABLE IF NOT EXISTS refund_request (
 id CHAR(36) NOT NULL PRIMARY KEY, order_id CHAR(36) NOT NULL, project_id CHAR(36) NOT NULL,
 requester_id CHAR(36) NOT NULL, amount DECIMAL(10,2) NOT NULL, reason VARCHAR(1000) NOT NULL,
 status VARCHAR(16) NOT NULL DEFAULT 'pending', mode VARCHAR(16) NOT NULL, idempotency_key CHAR(36) NOT NULL,
 created_at DATETIME NOT NULL, updated_at DATETIME NOT NULL, completed_at DATETIME DEFAULT NULL,
 UNIQUE KEY uq_refund_idempotency(requester_id,idempotency_key),
 KEY idx_refund_order(order_id,created_at,id), KEY idx_refund_project(project_id,status),
 CONSTRAINT ck_refund_amount CHECK(amount>0), CONSTRAINT ck_refund_status CHECK(status IN ('pending','processing','succeeded','failed')),
 CONSTRAINT ck_refund_mode CHECK(mode IN ('development','gateway')),
 CONSTRAINT fk_refund_order FOREIGN KEY(order_id) REFERENCES payment_order(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
 CONSTRAINT fk_refund_project FOREIGN KEY(project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
 CONSTRAINT fk_refund_requester FOREIGN KEY(requester_id) REFERENCES user(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
CREATE TABLE IF NOT EXISTS fund_disbursement (
 id CHAR(36) NOT NULL PRIMARY KEY, project_id CHAR(36) NOT NULL, requester_id CHAR(36) NOT NULL,
 amount DECIMAL(10,2) NOT NULL, purpose VARCHAR(1000) NOT NULL, evidence JSON NOT NULL,
 payout_reference VARCHAR(200) NOT NULL, status VARCHAR(16) NOT NULL DEFAULT 'requested', mode VARCHAR(16) NOT NULL,
 idempotency_key CHAR(36) NOT NULL, reviewer_id CHAR(36) DEFAULT NULL, review_note VARCHAR(1000) DEFAULT NULL,
 created_at DATETIME NOT NULL, updated_at DATETIME NOT NULL, completed_at DATETIME DEFAULT NULL,
 UNIQUE KEY uq_disbursement_idempotency(requester_id,idempotency_key),
 KEY idx_disbursement_project(project_id,status,created_at,id), KEY idx_disbursement_queue(status,created_at,id),
 CONSTRAINT ck_disbursement_amount CHECK(amount>0), CONSTRAINT ck_disbursement_status CHECK(status IN ('requested','approved','processing','succeeded','failed','rejected')),
 CONSTRAINT ck_disbursement_mode CHECK(mode IN ('development','gateway')),
 CONSTRAINT ck_disbursement_evidence CHECK(JSON_TYPE(evidence)='ARRAY'),
 CONSTRAINT fk_disbursement_project FOREIGN KEY(project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
 CONSTRAINT fk_disbursement_requester FOREIGN KEY(requester_id) REFERENCES user(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
 CONSTRAINT fk_disbursement_reviewer FOREIGN KEY(reviewer_id) REFERENCES user(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
CREATE TABLE IF NOT EXISTS fund_ledger (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, project_id CHAR(36) NOT NULL,
 source_type VARCHAR(16) NOT NULL, source_id CHAR(36) NOT NULL, amount DECIMAL(14,2) NOT NULL,
 created_at DATETIME NOT NULL,
 UNIQUE KEY uq_fund_ledger_source(source_type,source_id),
 KEY idx_fund_ledger_project(project_id,id),
 CONSTRAINT ck_fund_ledger_kind CHECK(source_type IN ('payment','refund','disbursement')),
 CONSTRAINT ck_fund_ledger_amount CHECK((source_type='payment' AND amount>0) OR (source_type IN ('refund','disbursement') AND amount<0)),
 CONSTRAINT fk_fund_ledger_project FOREIGN KEY(project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
CREATE TABLE IF NOT EXISTS payment_event (
 event_id VARCHAR(128) COLLATE utf8mb4_bin NOT NULL PRIMARY KEY,
 payload_hash CHAR(64) COLLATE utf8mb4_bin NOT NULL, source_type VARCHAR(16) NOT NULL, source_id CHAR(36) NOT NULL,
 status VARCHAR(16) NOT NULL, amount DECIMAL(10,2) NOT NULL, mode VARCHAR(16) NOT NULL,
 created_at DATETIME NOT NULL,
 KEY idx_payment_event_reference(source_type,source_id),
 CONSTRAINT ck_payment_event_type CHECK(source_type IN ('payment','refund','disbursement')),
 CONSTRAINT ck_payment_event_status CHECK(status IN ('succeeded','failed')),
 CONSTRAINT ck_payment_event_mode CHECK(mode IN ('development','gateway')),
 CONSTRAINT ck_payment_event_amount CHECK(amount>0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
INSERT INTO schema_migration(version,applied_at) VALUES('006_finance',UTC_TIMESTAMP())
ON DUPLICATE KEY UPDATE version=VALUES(version);
