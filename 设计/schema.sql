-- Cross-do schema for an empty MySQL >= 8.0.16 or MariaDB >= 11.8 database.
-- Re-import only after this schema or migration 002 completed successfully.
-- Existing databases without that marker must use migrations, never this bootstrap.
-- No production credentials, sample personal data, DROP TABLE or cascading deletes.
-- All application timestamps use UTC. Money uses exact decimal values.
SET NAMES utf8mb4;

-- Guard before any application-table DDL: CREATE TABLE IF NOT EXISTS alone would
-- leave old tables unchanged but incorrectly mark migration 002 as completed.
-- Do not use a client's --force option: a rejected bootstrap must stop here.
DROP PROCEDURE IF EXISTS crossdo_bootstrap_guard;
DELIMITER $$
CREATE PROCEDURE crossdo_bootstrap_guard()
BEGIN
  IF EXISTS (SELECT 1 FROM information_schema.tables
             WHERE table_schema=DATABASE() AND table_name='user') THEN
    IF NOT EXISTS (SELECT 1 FROM information_schema.tables
                   WHERE table_schema=DATABASE() AND table_name='schema_migration') THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Existing database requires migrations 001 and 002; schema.sql is only for bootstrap';
    ELSEIF NOT EXISTS (SELECT 1 FROM schema_migration WHERE version='002_schema_integrity') THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Migration 002 is incomplete; resume migrations instead of importing schema.sql';
    END IF;
  END IF;
END$$
DELIMITER ;
CALL crossdo_bootstrap_guard();
DROP PROCEDURE crossdo_bootstrap_guard;


CREATE TABLE IF NOT EXISTS `permission_group` (
  `id` int NOT NULL,
  `group_name` varchar(20) DEFAULT NULL,
  `group_illustrate` varchar(100) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `user` (
  `user_id` char(36) NOT NULL,
  `user_permission` int NOT NULL,
  `user_name` varchar(24) DEFAULT NULL,
  `user_password` varchar(64) DEFAULT NULL,
  `avatar` varchar(64) DEFAULT NULL,
  `real_name` varchar(20) DEFAULT NULL,
  `id_card` char(18) DEFAULT NULL,
  `personal_note` varchar(300) DEFAULT NULL,
  `real_name_authentication` tinyint(1) NOT NULL DEFAULT 0,
  `user_status` tinyint NOT NULL DEFAULT 1,
  `existing_projects` tinyint(1) NOT NULL DEFAULT 0,
  `project_id` char(36) DEFAULT NULL,
  `weixin_openid` char(36) DEFAULT NULL,
  `phone_number` char(11) DEFAULT NULL,
  `user_age` tinyint DEFAULT NULL,
  `registration_date` datetime DEFAULT NULL,
  `login_time` datetime DEFAULT NULL,
  `current_dispute_id` char(36) DEFAULT NULL,
  `total_donation_amount` DECIMAL(10,2) DEFAULT NULL,
  UNIQUE KEY `uq_user_phone_number` (`phone_number`),
  KEY `idx_user_review_queue` (`user_permission`,`user_status`,`user_id`),
  KEY `idx_user_current_project` (`project_id`,`user_id`),
  CONSTRAINT `ck_user_status` CHECK (user_status BETWEEN 0 AND 13),
  CONSTRAINT `ck_user_verified` CHECK (real_name_authentication IN (0,1)),
  CONSTRAINT `ck_user_project_flag` CHECK (existing_projects IN (0,1) AND existing_projects = (project_id IS NOT NULL)),
  CONSTRAINT `ck_user_total_donations` CHECK (total_donation_amount IS NULL OR total_donation_amount >= 0),
  CONSTRAINT `fk_user_permission` FOREIGN KEY (`user_permission`) REFERENCES `permission_group` (`id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  PRIMARY KEY (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `project` (
  `project_id` char(36) NOT NULL,
  `user_id` char(36) NOT NULL,
  `user_name` varchar(24) DEFAULT NULL,
  `user_age` tinyint DEFAULT NULL,
  `real_name` varchar(20) DEFAULT NULL,
  `project_status` tinyint NOT NULL DEFAULT 1,
  `verification_record` text,
  `current_volunteer` char(36) DEFAULT NULL,
  `previous_volunteers` text,
  `question_is_volunteer_assigned` tinyint(1) DEFAULT NULL,
  `current_dispute_id` char(36) DEFAULT NULL,
  `project_create_time` datetime DEFAULT NULL,
  `donation_pool_limit` DECIMAL(10,2) NOT NULL DEFAULT 0.00,
  `current_donations_received` DECIMAL(10,2) NOT NULL DEFAULT 0.00,
  `current_used_donations` DECIMAL(10,2) NOT NULL DEFAULT 0.00,
  `last_update_time` datetime DEFAULT NULL,
  `pathography` text,
  KEY `idx_project_user_id` (`user_id`),
  KEY `idx_project_queue` (`project_status`, `project_create_time`),
  `submission_no` INT UNSIGNED NOT NULL DEFAULT 0,
  `active_owner_id` CHAR(36) GENERATED ALWAYS AS (IF(project_status <> 13,RTRIM(user_id),NULL)) STORED,
  UNIQUE KEY `uq_project_owner_reference` (`project_id`,`user_id`),
  UNIQUE KEY `uq_project_active_owner` (`active_owner_id`),
  KEY `idx_project_reviewer_queue` (`current_volunteer`,`project_status`,`project_create_time`,`project_id`),
  CONSTRAINT `ck_project_status` CHECK (project_status BETWEEN 0 AND 13),
  CONSTRAINT `ck_project_amounts` CHECK (donation_pool_limit >= 0 AND current_donations_received >= 0 AND current_used_donations >= 0 AND current_used_donations <= current_donations_received),
  CONSTRAINT `ck_project_submitted_target` CHECK (project_status NOT IN (0,2,3,6,7) OR donation_pool_limit > 0),
  CONSTRAINT `ck_project_assigned_flag` CHECK (question_is_volunteer_assigned IS NULL OR question_is_volunteer_assigned IN (0,1)),
  CONSTRAINT `ck_project_history` CHECK (CASE WHEN verification_record IS NULL OR verification_record = '' THEN TRUE WHEN JSON_VALID(verification_record) THEN JSON_TYPE(verification_record) = 'ARRAY' ELSE FALSE END = 1),
  CONSTRAINT `ck_project_previous` CHECK (CASE WHEN previous_volunteers IS NULL OR previous_volunteers = '' THEN TRUE WHEN JSON_VALID(previous_volunteers) THEN JSON_TYPE(previous_volunteers) = 'ARRAY' ELSE FALSE END = 1),
  CONSTRAINT `ck_project_pathography` CHECK (CASE WHEN pathography IS NULL OR pathography = '' THEN TRUE WHEN JSON_VALID(pathography) THEN JSON_TYPE(pathography) = 'ARRAY' ELSE FALSE END = 1),
  CONSTRAINT `fk_project_owner` FOREIGN KEY (`user_id`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `fk_project_reviewer` FOREIGN KEY (`current_volunteer`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  PRIMARY KEY (`project_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `patient_application` (
  `user_id` CHAR(36) NOT NULL,
  `materials` TEXT NOT NULL,
  `current_volunteer` CHAR(36) DEFAULT NULL,
  `previous_volunteers` TEXT NOT NULL,
  `verification_record` TEXT NOT NULL,
  `submitted_at` DATETIME NOT NULL,
  `last_change_time` DATETIME NOT NULL,
  KEY `idx_application_submitted` (`submitted_at`,`user_id`),
  KEY `idx_application_reviewer_queue` (`current_volunteer`,`submitted_at`,`user_id`),
  CONSTRAINT `ck_application_materials` CHECK (CASE WHEN JSON_VALID(materials) THEN JSON_TYPE(materials) = 'ARRAY' ELSE FALSE END = 1),
  CONSTRAINT `ck_application_history` CHECK (CASE WHEN verification_record IS NULL OR verification_record = '' THEN TRUE WHEN JSON_VALID(verification_record) THEN JSON_TYPE(verification_record) = 'ARRAY' ELSE FALSE END = 1),
  CONSTRAINT `ck_application_previous` CHECK (CASE WHEN previous_volunteers IS NULL OR previous_volunteers = '' THEN TRUE WHEN JSON_VALID(previous_volunteers) THEN JSON_TYPE(previous_volunteers) = 'ARRAY' ELSE FALSE END = 1),
  CONSTRAINT `fk_application_user` FOREIGN KEY (`user_id`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `fk_application_reviewer` FOREIGN KEY (`current_volunteer`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  PRIMARY KEY (`user_id`),
  KEY `idx_application_volunteer` (`current_volunteer`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- The current-project pointer closes a cycle: insert user with no project, then
-- insert project, then link user in one transaction. Never disable foreign keys.
SET @crossdo_current_project_fk_sql = IF(
  EXISTS(SELECT 1 FROM information_schema.table_constraints
         WHERE constraint_schema = DATABASE() AND table_name = 'user'
           AND constraint_name = 'fk_user_current_project'),
  'SELECT 1',
  'ALTER TABLE `user` ADD CONSTRAINT `fk_user_current_project` FOREIGN KEY (`project_id`,`user_id`) REFERENCES `project` (`project_id`,`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT'
);
PREPARE crossdo_current_project_fk FROM @crossdo_current_project_fk_sql;
EXECUTE crossdo_current_project_fk;
DEALLOCATE PREPARE crossdo_current_project_fk;

CREATE TABLE IF NOT EXISTS `application_review_event` (
  `event_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `user_id` CHAR(36) NOT NULL,
  `sequence_no` BIGINT UNSIGNED NOT NULL,
  `actor_id` CHAR(36) DEFAULT NULL,
  `decision` VARCHAR(20) DEFAULT NULL,
  `recorded_at` DATETIME DEFAULT NULL,
  `record` JSON NOT NULL,
  `legacy_import` TINYINT NOT NULL DEFAULT 0,
  PRIMARY KEY (`event_id`),
  UNIQUE KEY `uq_application_event_sequence` (`user_id`,`sequence_no`),
  KEY `idx_application_event_actor` (`actor_id`),
  CONSTRAINT `ck_application_event_sequence` CHECK (sequence_no > 0),
  CONSTRAINT `ck_application_event_legacy` CHECK (legacy_import IN (0,1)),
  CONSTRAINT `fk_application_event_parent` FOREIGN KEY (`user_id`) REFERENCES `patient_application` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `fk_application_event_actor` FOREIGN KEY (`actor_id`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `project_review_event` (
  `event_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `project_id` CHAR(36) NOT NULL,
  `sequence_no` BIGINT UNSIGNED NOT NULL,
  `actor_id` CHAR(36) DEFAULT NULL,
  `decision` VARCHAR(20) DEFAULT NULL,
  `recorded_at` DATETIME DEFAULT NULL,
  `record` JSON NOT NULL,
  `legacy_import` TINYINT NOT NULL DEFAULT 0,
  PRIMARY KEY (`event_id`),
  UNIQUE KEY `uq_project_event_sequence` (`project_id`,`sequence_no`),
  KEY `idx_project_event_actor` (`actor_id`),
  CONSTRAINT `ck_project_event_sequence` CHECK (sequence_no > 0),
  CONSTRAINT `ck_project_event_legacy` CHECK (legacy_import IN (0,1)),
  CONSTRAINT `fk_project_event_parent` FOREIGN KEY (`project_id`) REFERENCES `project` (`project_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `fk_project_event_actor` FOREIGN KEY (`actor_id`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `project_pathography_entry` (
  `entry_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `project_id` CHAR(36) NOT NULL,
  `submission_no` INT UNSIGNED NOT NULL,
  `sequence_no` BIGINT UNSIGNED NOT NULL,
  `record` JSON NOT NULL,
  PRIMARY KEY (`entry_id`),
  UNIQUE KEY `uq_project_pathography_sequence` (`project_id`,`submission_no`,`sequence_no`),
  CONSTRAINT `ck_project_pathography_sequence` CHECK (sequence_no > 0),
  CONSTRAINT `fk_project_pathography_parent` FOREIGN KEY (`project_id`) REFERENCES `project` (`project_id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `dispute` (
  `current_dispute_id` char(36) NOT NULL,
  `dispute_types` int DEFAULT NULL,
  `user_id` char(36) DEFAULT NULL,
  `project_id` char(36) DEFAULT NULL,
  `dispute_status` int DEFAULT NULL,
  `reason` text,
  PRIMARY KEY (`current_dispute_id`),
  KEY `idx_user_id` (`user_id`),
  KEY `idx_project_id` (`project_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `ledger_investigations` (
  `investigation_id` CHAR(36)  NOT NULL,            -- 主键：流水问题人工介入调查唯一ID
  `ledger_id` CHAR(36)  DEFAULT NULL,               -- 资金流水唯一ID
  `user_id` CHAR(36)  DEFAULT NULL,                 -- 介入发起用户UUID
  `user_note` text  DEFAULT NULL,                   -- 用户反馈问题记录
  `current_operation` char(36) DEFAULT NULL,        -- 当前负责运维ID
  `conclusion_note` text DEFAULT NULL,              -- 当前事件结论
  PRIMARY KEY (`investigation_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `donation_ledger` (
  `ledger_id` CHAR(36)  NOT NULL,                   -- 主键：资金流水唯一ID
  `user_id` CHAR(36)  DEFAULT NULL,                 -- 发起人（捐赠者/患者/操作者）
  `sufferer_user_id` CHAR(36)  DEFAULT NULL,        -- 项目受助者ID
  `project_id` CHAR(36)  DEFAULT NULL,              -- 所属项目（含特殊池）
  `donor_user_name` VARCHAR(24) DEFAULT NULL,       -- 捐赠者用户名
  `sufferer_real_name` VARCHAR(20) DEFAULT NULL,    -- 患者实名（仅用于存档）
  `sufferer_user_name` VARCHAR(24) DEFAULT NULL,    -- 患者用户名（辅助显示）
  `target_user_id` CHAR(36)  DEFAULT NULL,          -- 发生项目结束退款时，用以记录退款人ID
  `amount` DECIMAL(10,2) NOT NULL,                  -- 金额，正负表示入账/出账
  `transaction_time` DATETIME  NOT NULL,            -- 发生时间（统一命名）
  `change_time` DATETIME  DEFAULT NULL,             -- 最近修改时间
  `note` VARCHAR(200) DEFAULT NULL,                 -- 备注
  `payment_method` TINYINT   DEFAULT NULL,          -- 支付方式（0：支付宝 1：微信 2：银行卡）
  `method_id` CHAR(36)  DEFAULT NULL,               -- 支付平台返回的ID，如微信单号
  `transaction_type` TINYINT   NOT NULL,            -- 记录性质（0：捐赠 1：退款待选择 2：退款 3：转应急池 4：患者使用 5：患者应急使用 6：平台使用 7：项目援助）
  `status` TINYINT   DEFAULT 1,                     -- 账单状态（0：成功 1：等待支付 2：支付失败 3：处理中 4：处理失败 5：退款中 6：退款失败 7：退款完成 8：发款中 9：发款失败 10：发起人工介入 11：人工介入中 12：人工介入完成 13：应急使用未核销 14：应急使用核销完成）
  `investigation_id` CHAR(36)  DEFAULT NULL,        -- 人工介入调查ID
  `volunteer_audit_id` CHAR(36) DEFAULT NULL,       -- 关联的志愿者审计记录ID，仅有志愿者介入时有值
  PRIMARY KEY (`ledger_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `donation_volunteer_audit` (
  `audit_id` CHAR(36)  NOT NULL,              -- 通常等于 ledger_id，特殊情况下也可单独生成
  `current_volunteer` CHAR(36)  DEFAULT NULL, -- 当前志愿者 ID
  `previous_volunteers` TEXT  DEFAULT NULL,   -- 历任志愿者 UUID 数组（JSON）
  `verification_record` TEXT  DEFAULT NULL,   -- 审计详情，JSON 格式
  `last_change_time` DATETIME  NOT NULL,      -- 最近修改时间
  PRIMARY KEY (`audit_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `service` (
  `service_name` VARCHAR(64) NOT NULL,
  `service_passwd` VARCHAR(255) NOT NULL,
  PRIMARY KEY (`service_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `schema_migration` (
  `version` VARCHAR(64) NOT NULL,
  `applied_at` DATETIME NOT NULL,
  PRIMARY KEY (`version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

INSERT INTO `permission_group` (`id`, `group_name`, `group_illustrate`) VALUES
 (1, 'admin', '管理员（保留）'),
 (2, 'donor', '捐赠者'),
 (3, 'sufferer', '患者'),
 (5, 'volunteer', '志愿者'),
 (6, 'long_term_donors', '长期捐赠者'),
 (7, 'senior_volunteer', '资深志愿者')
ON DUPLICATE KEY UPDATE `id` = VALUES(`id`);

INSERT INTO schema_migration(version,applied_at) VALUES ('002_schema_integrity',UTC_TIMESTAMP())
ON DUPLICATE KEY UPDATE version=VALUES(version);

-- BEGIN ADDITIVE RELEASE MODULES (generated by tools/build_schema.py)

-- Source: migrations/003_operations.sql
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


-- Source: migrations/004_account_security.sql
-- Requires 003_operations.sql. Stop writers during schema upgrades.
-- Existing accounts retain their credentials; phone verification begins at 0.
SET @crossdo_phone_verified_sql = IF(
  EXISTS(SELECT 1 FROM information_schema.columns WHERE table_schema=DATABASE()
         AND table_name='user' AND column_name='phone_verified'),
  'SELECT 1',
  'ALTER TABLE `user` ADD COLUMN `phone_verified` TINYINT NOT NULL DEFAULT 0'
);
PREPARE crossdo_security_column FROM @crossdo_phone_verified_sql;
EXECUTE crossdo_security_column;
DEALLOCATE PREPARE crossdo_security_column;
SET @crossdo_phone_verified_check_sql = IF(
  EXISTS(SELECT 1 FROM information_schema.table_constraints WHERE constraint_schema=DATABASE()
         AND table_name='user' AND constraint_name='ck_user_phone_verified'),
  'SELECT 1',
  'ALTER TABLE `user` ADD CONSTRAINT `ck_user_phone_verified` CHECK (phone_verified IN (0,1))'
);
PREPARE crossdo_security_check FROM @crossdo_phone_verified_check_sql;
EXECUTE crossdo_security_check;
DEALLOCATE PREPARE crossdo_security_check;
INSERT INTO schema_migration(version,applied_at) VALUES ('004_account_security',UTC_TIMESTAMP())
ON DUPLICATE KEY UPDATE version=VALUES(version);


-- Source: migrations/005_files.sql
-- Apply after 004_account_security.sql with writers stopped. Repeatable and additive.
-- No file bytes are stored in SQL or served from a static web directory.
SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS uploaded_file (
  id CHAR(36) NOT NULL,
  owner_id CHAR(36) NOT NULL,
  purpose VARCHAR(16) NOT NULL,
  project_id CHAR(36) DEFAULT NULL,
  storage_key CHAR(36) NOT NULL,
  original_name VARCHAR(255) NOT NULL,
  mime_type VARCHAR(40) NOT NULL,
  byte_size BIGINT UNSIGNED NOT NULL,
  sha256 CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  created_at DATETIME NOT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uq_uploaded_file_storage (storage_key),
  KEY idx_uploaded_file_owner (owner_id,created_at,id),
  KEY idx_uploaded_file_project (project_id),
  CONSTRAINT ck_uploaded_file_purpose CHECK (purpose IN ('application','project','case')),
  CONSTRAINT ck_uploaded_file_target CHECK ((purpose IN ('application','case') AND project_id IS NULL) OR (purpose='project' AND project_id IS NOT NULL)),
  CONSTRAINT ck_uploaded_file_mime CHECK (mime_type IN ('image/png','image/jpeg','application/pdf')),
  CONSTRAINT ck_uploaded_file_size CHECK (byte_size > 0 AND byte_size <= 10485760),
  CONSTRAINT fk_uploaded_file_owner FOREIGN KEY (owner_id) REFERENCES user(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_uploaded_file_project FOREIGN KEY (project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- A file may be reused by its owner in later submissions. Only a current
-- published project version grants anonymous download; private proofs never do.
CREATE TABLE IF NOT EXISTS file_usage (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  file_id CHAR(36) NOT NULL,
  application_user_id CHAR(36) DEFAULT NULL,
  project_id CHAR(36) DEFAULT NULL,
  submission_no INT UNSIGNED NOT NULL DEFAULT 0,
  created_at DATETIME NOT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uq_file_application_usage (file_id,application_user_id),
  UNIQUE KEY uq_file_project_usage (file_id,project_id,submission_no),
  KEY idx_file_application_target (application_user_id,file_id),
  KEY idx_file_project_target (project_id,submission_no,file_id),
  CONSTRAINT ck_file_usage_target CHECK ((application_user_id IS NOT NULL AND project_id IS NULL AND submission_no=0)
      OR (application_user_id IS NULL AND project_id IS NOT NULL)),
  CONSTRAINT fk_file_usage_file FOREIGN KEY (file_id) REFERENCES uploaded_file(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_file_usage_application_user FOREIGN KEY (application_user_id) REFERENCES user(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT fk_file_usage_project FOREIGN KEY (project_id) REFERENCES project(project_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

INSERT INTO schema_migration(version,applied_at) VALUES ('005_files',UTC_TIMESTAMP())
ON DUPLICATE KEY UPDATE version=VALUES(version);


-- Source: migrations/006_finance.sql
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
