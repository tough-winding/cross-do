-- TEST FIXTURE: workflow schema immediately before migration 002.
-- Keep this historical shape unchanged; deployment uses 设计/schema.sql.
-- Cross-do schema for an empty MySQL 8 database. Select the database before importing.
-- This file contains no production credentials or example personal data and never drops tables.
SET NAMES utf8mb4;

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

CREATE TABLE IF NOT EXISTS `permission_group` (
  `id` int NOT NULL,
  `group_name` varchar(20) DEFAULT NULL,
  `group_illustrate` varchar(100) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `project` (
  `project_id` char(36) NOT NULL,
  `user_id` char(36) NOT NULL,
  `user_name` varchar(24) DEFAULT NULL,
  `user_age` tinyint DEFAULT NULL,
  `real_name` varchar(20) DEFAULT NULL,
  `project_status` tinyint DEFAULT 1,
  `verification_record` text,
  `current_volunteer` char(36) DEFAULT NULL,
  `previous_volunteers` text,
  `question_is_volunteer_assigned` tinyint(1) DEFAULT NULL,
  `current_dispute_id` char(36) DEFAULT NULL,
  `project_create_time` datetime DEFAULT NULL,
  `donation_pool_limit` DECIMAL(10,2) DEFAULT NULL,
  `current_donations_received` DECIMAL(10,2) DEFAULT NULL,
  `current_used_donations` DECIMAL(10,2) DEFAULT NULL,
  `last_update_time` datetime DEFAULT NULL,
  `pathography` text,
  KEY `idx_project_user_id` (`user_id`),
  KEY `idx_project_queue` (`project_status`, `project_create_time`),
  PRIMARY KEY (`project_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `user` (
  `user_id` char(36) NOT NULL,
  `user_permission` int DEFAULT NULL,
  `user_name` varchar(24) DEFAULT NULL,
  `user_password` varchar(64) DEFAULT NULL,
  `avatar` varchar(64) DEFAULT NULL,
  `real_name` varchar(20) DEFAULT NULL,
  `id_card` char(18) DEFAULT NULL,
  `personal_note` varchar(300) DEFAULT NULL,
  `real_name_authentication` tinyint(1) DEFAULT NULL,
  `user_status` tinyint DEFAULT NULL,
  `existing_projects` tinyint(1) DEFAULT NULL,
  `project_id` char(36) DEFAULT NULL,
  `weixin_openid` char(36) DEFAULT NULL,
  `phone_number` char(11) DEFAULT NULL,
  `user_age` tinyint DEFAULT NULL,
  `registration_date` datetime DEFAULT NULL,
  `login_time` datetime DEFAULT NULL,
  `current_dispute_id` char(36) DEFAULT NULL,
  `total_donation_amount` DECIMAL(10,2) DEFAULT NULL,
  UNIQUE KEY `uq_user_phone_number` (`phone_number`),
  PRIMARY KEY (`user_id`)
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

CREATE TABLE IF NOT EXISTS `patient_application` (
  `user_id` CHAR(36) NOT NULL,
  `materials` TEXT NOT NULL,
  `current_volunteer` CHAR(36) DEFAULT NULL,
  `previous_volunteers` TEXT NOT NULL,
  `verification_record` TEXT NOT NULL,
  `submitted_at` DATETIME NOT NULL,
  `last_change_time` DATETIME NOT NULL,
  PRIMARY KEY (`user_id`),
  KEY `idx_application_volunteer` (`current_volunteer`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

INSERT INTO `permission_group` (`id`, `group_name`, `group_illustrate`) VALUES
 (1, 'admin', '管理员（保留）'),
 (2, 'donor', '捐赠者'),
 (3, 'sufferer', '患者'),
 (5, 'volunteer', '志愿者'),
 (6, 'long_term_donors', '长期捐赠者'),
 (7, 'senior_volunteer', '资深志愿者')
ON DUPLICATE KEY UPDATE `id` = VALUES(`id`);
