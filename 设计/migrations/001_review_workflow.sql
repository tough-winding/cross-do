-- Apply to an existing cross-do MySQL 8 database after a backup.
-- Existing phone duplicates are intentionally rejected by the unique-index creation.
-- Find them before migration:
-- SELECT phone_number, COUNT(*) FROM user WHERE phone_number IS NOT NULL
-- GROUP BY phone_number HAVING COUNT(*) > 1;
-- CREATE TABLE is repeatable. The index statement checks information_schema so this
-- migration can also run after schema.sql or after an earlier successful application.

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

SET @crossdo_phone_index_sql = IF(
  EXISTS(SELECT 1 FROM information_schema.statistics
         WHERE table_schema = DATABASE() AND table_name = 'user'
           AND index_name = 'uq_user_phone_number'),
  'SELECT 1',
  'ALTER TABLE `user` ADD UNIQUE KEY `uq_user_phone_number` (`phone_number`)'
);
PREPARE crossdo_phone_index FROM @crossdo_phone_index_sql;
EXECUTE crossdo_phone_index;
DEALLOCATE PREPARE crossdo_phone_index;

-- Owner lookups must use a narrow index when the workflow checks existing projects
-- under a transaction. The queue index also bounds scans for pending/active projects.
SET @crossdo_project_owner_sql = IF(
  EXISTS(SELECT 1 FROM information_schema.statistics
         WHERE table_schema = DATABASE() AND table_name = 'project'
           AND index_name = 'idx_project_user_id'),
  'SELECT 1',
  'ALTER TABLE `project` ADD KEY `idx_project_user_id` (`user_id`)'
);
PREPARE crossdo_project_owner_index FROM @crossdo_project_owner_sql;
EXECUTE crossdo_project_owner_index;
DEALLOCATE PREPARE crossdo_project_owner_index;

SET @crossdo_project_queue_sql = IF(
  EXISTS(SELECT 1 FROM information_schema.statistics
         WHERE table_schema = DATABASE() AND table_name = 'project'
           AND index_name = 'idx_project_queue'),
  'SELECT 1',
  'ALTER TABLE `project` ADD KEY `idx_project_queue` (`project_status`,`project_create_time`)'
);
PREPARE crossdo_project_queue_index FROM @crossdo_project_queue_sql;
EXECUTE crossdo_project_queue_index;
DEALLOCATE PREPARE crossdo_project_queue_index;
