-- Upgrade after 001_review_workflow.sql. MySQL >= 8.0.16 / MariaDB >= 11.8.
-- Back up first and STOP ALL WRITERS, including old DBOP routes, until deployment.
-- Do not use a client's --force option: preflight errors must abort the script.
-- DDL auto-commits; a failure can leave added schema, but never silently repairs or
-- removes historical data. Re-running resumes missing DDL and atomic backfill.
-- Legacy TEXT columns remain untouched migration snapshots. After completion,
-- runtime uses event/entry tables; re-running never imports snapshots again.
SET NAMES utf8mb4;

-- Run after 001_review_workflow.sql, with application writers stopped.
-- Counts only: no identity, phone, medical content or credential values are printed.
-- A nonzero count requires operator reconciliation; no rows are changed here.
DROP PROCEDURE IF EXISTS crossdo_preflight_002;
DELIMITER $$
CREATE PROCEDURE crossdo_preflight_002()
BEGIN
  DECLARE error_count BIGINT DEFAULT 0;
  SELECT issue,affected_rows FROM (
    SELECT 'user_required_or_status' AS issue, (SELECT COUNT(*) FROM `user` WHERE user_permission IS NULL OR user_status IS NULL OR user_status NOT BETWEEN 0 AND 13 OR real_name_authentication IS NULL OR real_name_authentication NOT IN (0,1) OR existing_projects IS NULL OR existing_projects NOT IN (0,1)) AS affected_rows
    UNION ALL
    SELECT 'user_unknown_permission' AS issue, (SELECT COUNT(*) FROM `user` u LEFT JOIN permission_group p ON p.id=u.user_permission WHERE u.user_permission IS NOT NULL AND p.id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'user_project_flag_mismatch' AS issue, (SELECT COUNT(*) FROM `user` WHERE existing_projects <> (project_id IS NOT NULL) OR project_id = '') AS affected_rows
    UNION ALL
    SELECT 'user_project_missing_or_wrong_owner' AS issue, (SELECT COUNT(*) FROM `user` u LEFT JOIN project p ON p.project_id=u.project_id AND p.user_id=u.user_id WHERE u.project_id IS NOT NULL AND p.project_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'user_negative_donations' AS issue, (SELECT COUNT(*) FROM `user` WHERE total_donation_amount < 0) AS affected_rows
    UNION ALL
    SELECT 'project_missing_owner' AS issue, (SELECT COUNT(*) FROM project p LEFT JOIN `user` u ON u.user_id=p.user_id WHERE u.user_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'project_missing_reviewer' AS issue, (SELECT COUNT(*) FROM project p LEFT JOIN `user` u ON u.user_id=p.current_volunteer WHERE p.current_volunteer IS NOT NULL AND u.user_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'project_invalid_status_or_flag' AS issue, (SELECT COUNT(*) FROM project WHERE project_status IS NULL OR project_status NOT BETWEEN 0 AND 13 OR (question_is_volunteer_assigned IS NOT NULL AND question_is_volunteer_assigned NOT IN (0,1))) AS affected_rows
    UNION ALL
    SELECT 'project_invalid_amounts' AS issue, (SELECT COUNT(*) FROM project WHERE donation_pool_limit IS NULL OR current_donations_received IS NULL OR current_used_donations IS NULL OR donation_pool_limit < 0 OR current_donations_received < 0 OR current_used_donations < 0 OR current_used_donations > current_donations_received OR (project_status IN (0,2,3,6,7) AND donation_pool_limit <= 0)) AS affected_rows
    UNION ALL
    SELECT 'project_multiple_active_for_owner' AS issue, (SELECT COUNT(*) FROM (SELECT user_id FROM project WHERE project_status <> 13 GROUP BY user_id HAVING COUNT(*) > 1) duplicate_owners) AS affected_rows
    UNION ALL
    SELECT 'application_missing_user' AS issue, (SELECT COUNT(*) FROM patient_application a LEFT JOIN `user` u ON u.user_id=a.user_id WHERE u.user_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'application_missing_reviewer' AS issue, (SELECT COUNT(*) FROM patient_application a LEFT JOIN `user` u ON u.user_id=a.current_volunteer WHERE a.current_volunteer IS NOT NULL AND u.user_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'application_invalid_materials' AS issue, (SELECT COUNT(*) FROM patient_application WHERE CASE WHEN JSON_VALID(materials) THEN JSON_TYPE(materials) <> 'ARRAY' ELSE 1 END) AS affected_rows
    UNION ALL
    SELECT 'application_invalid_legacy_json' AS issue, (SELECT COUNT(*) FROM patient_application WHERE CASE WHEN verification_record IS NULL OR verification_record = '' THEN 0 WHEN JSON_VALID(verification_record) THEN JSON_TYPE(verification_record) <> 'ARRAY' ELSE 1 END OR CASE WHEN previous_volunteers IS NULL OR previous_volunteers = '' THEN 0 WHEN JSON_VALID(previous_volunteers) THEN JSON_TYPE(previous_volunteers) <> 'ARRAY' ELSE 1 END) AS affected_rows
    UNION ALL
    SELECT 'project_invalid_legacy_json' AS issue, (SELECT COUNT(*) FROM project WHERE CASE WHEN verification_record IS NULL OR verification_record = '' THEN 0 WHEN JSON_VALID(verification_record) THEN JSON_TYPE(verification_record) <> 'ARRAY' ELSE 1 END OR CASE WHEN previous_volunteers IS NULL OR previous_volunteers = '' THEN 0 WHEN JSON_VALID(previous_volunteers) THEN JSON_TYPE(previous_volunteers) <> 'ARRAY' ELSE 1 END OR CASE WHEN pathography IS NULL OR pathography = '' THEN 0 WHEN JSON_VALID(pathography) THEN JSON_TYPE(pathography) <> 'ARRAY' ELSE 1 END) AS affected_rows
  ) issues WHERE affected_rows > 0 ORDER BY issue;
  SELECT COALESCE(SUM(affected_rows),0) INTO error_count FROM (
    SELECT 'user_required_or_status' AS issue, (SELECT COUNT(*) FROM `user` WHERE user_permission IS NULL OR user_status IS NULL OR user_status NOT BETWEEN 0 AND 13 OR real_name_authentication IS NULL OR real_name_authentication NOT IN (0,1) OR existing_projects IS NULL OR existing_projects NOT IN (0,1)) AS affected_rows
    UNION ALL
    SELECT 'user_unknown_permission' AS issue, (SELECT COUNT(*) FROM `user` u LEFT JOIN permission_group p ON p.id=u.user_permission WHERE u.user_permission IS NOT NULL AND p.id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'user_project_flag_mismatch' AS issue, (SELECT COUNT(*) FROM `user` WHERE existing_projects <> (project_id IS NOT NULL) OR project_id = '') AS affected_rows
    UNION ALL
    SELECT 'user_project_missing_or_wrong_owner' AS issue, (SELECT COUNT(*) FROM `user` u LEFT JOIN project p ON p.project_id=u.project_id AND p.user_id=u.user_id WHERE u.project_id IS NOT NULL AND p.project_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'user_negative_donations' AS issue, (SELECT COUNT(*) FROM `user` WHERE total_donation_amount < 0) AS affected_rows
    UNION ALL
    SELECT 'project_missing_owner' AS issue, (SELECT COUNT(*) FROM project p LEFT JOIN `user` u ON u.user_id=p.user_id WHERE u.user_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'project_missing_reviewer' AS issue, (SELECT COUNT(*) FROM project p LEFT JOIN `user` u ON u.user_id=p.current_volunteer WHERE p.current_volunteer IS NOT NULL AND u.user_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'project_invalid_status_or_flag' AS issue, (SELECT COUNT(*) FROM project WHERE project_status IS NULL OR project_status NOT BETWEEN 0 AND 13 OR (question_is_volunteer_assigned IS NOT NULL AND question_is_volunteer_assigned NOT IN (0,1))) AS affected_rows
    UNION ALL
    SELECT 'project_invalid_amounts' AS issue, (SELECT COUNT(*) FROM project WHERE donation_pool_limit IS NULL OR current_donations_received IS NULL OR current_used_donations IS NULL OR donation_pool_limit < 0 OR current_donations_received < 0 OR current_used_donations < 0 OR current_used_donations > current_donations_received OR (project_status IN (0,2,3,6,7) AND donation_pool_limit <= 0)) AS affected_rows
    UNION ALL
    SELECT 'project_multiple_active_for_owner' AS issue, (SELECT COUNT(*) FROM (SELECT user_id FROM project WHERE project_status <> 13 GROUP BY user_id HAVING COUNT(*) > 1) duplicate_owners) AS affected_rows
    UNION ALL
    SELECT 'application_missing_user' AS issue, (SELECT COUNT(*) FROM patient_application a LEFT JOIN `user` u ON u.user_id=a.user_id WHERE u.user_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'application_missing_reviewer' AS issue, (SELECT COUNT(*) FROM patient_application a LEFT JOIN `user` u ON u.user_id=a.current_volunteer WHERE a.current_volunteer IS NOT NULL AND u.user_id IS NULL) AS affected_rows
    UNION ALL
    SELECT 'application_invalid_materials' AS issue, (SELECT COUNT(*) FROM patient_application WHERE CASE WHEN JSON_VALID(materials) THEN JSON_TYPE(materials) <> 'ARRAY' ELSE 1 END) AS affected_rows
    UNION ALL
    SELECT 'application_invalid_legacy_json' AS issue, (SELECT COUNT(*) FROM patient_application WHERE CASE WHEN verification_record IS NULL OR verification_record = '' THEN 0 WHEN JSON_VALID(verification_record) THEN JSON_TYPE(verification_record) <> 'ARRAY' ELSE 1 END OR CASE WHEN previous_volunteers IS NULL OR previous_volunteers = '' THEN 0 WHEN JSON_VALID(previous_volunteers) THEN JSON_TYPE(previous_volunteers) <> 'ARRAY' ELSE 1 END) AS affected_rows
    UNION ALL
    SELECT 'project_invalid_legacy_json' AS issue, (SELECT COUNT(*) FROM project WHERE CASE WHEN verification_record IS NULL OR verification_record = '' THEN 0 WHEN JSON_VALID(verification_record) THEN JSON_TYPE(verification_record) <> 'ARRAY' ELSE 1 END OR CASE WHEN previous_volunteers IS NULL OR previous_volunteers = '' THEN 0 WHEN JSON_VALID(previous_volunteers) THEN JSON_TYPE(previous_volunteers) <> 'ARRAY' ELSE 1 END OR CASE WHEN pathography IS NULL OR pathography = '' THEN 0 WHEN JSON_VALID(pathography) THEN JSON_TYPE(pathography) <> 'ARRAY' ELSE 1 END) AS affected_rows
  ) issues;
  IF error_count > 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Migration 002 preflight failed; reconcile reported categories before retrying';
  END IF;
END$$
DELIMITER ;
CALL crossdo_preflight_002();
DROP PROCEDURE crossdo_preflight_002;

CREATE TABLE IF NOT EXISTS `schema_migration` (
  `version` VARCHAR(64) NOT NULL,
  `applied_at` DATETIME NOT NULL,
  PRIMARY KEY (`version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

DROP PROCEDURE IF EXISTS crossdo_add_index_002;
DROP PROCEDURE IF EXISTS crossdo_add_constraint_002;
DROP PROCEDURE IF EXISTS crossdo_add_column_002;
DROP PROCEDURE IF EXISTS crossdo_migrate_002;
DELIMITER $$
CREATE PROCEDURE crossdo_add_index_002(IN p_table VARCHAR(64), IN p_name VARCHAR(64), IN p_definition TEXT)
BEGIN
  IF NOT EXISTS (SELECT 1 FROM information_schema.statistics WHERE table_schema=DATABASE() AND table_name=p_table AND index_name=p_name) THEN
    SET @crossdo_ddl_002=CONCAT('ALTER TABLE `',p_table,'` ADD ',p_definition);
    PREPARE crossdo_ddl FROM @crossdo_ddl_002;
    EXECUTE crossdo_ddl;
    DEALLOCATE PREPARE crossdo_ddl;
  END IF;
END$$
CREATE PROCEDURE crossdo_add_constraint_002(IN p_table VARCHAR(64), IN p_name VARCHAR(64), IN p_definition TEXT)
BEGIN
  IF NOT EXISTS (SELECT 1 FROM information_schema.table_constraints WHERE constraint_schema=DATABASE() AND table_name=p_table AND constraint_name=p_name) THEN
    SET @crossdo_ddl_002=CONCAT('ALTER TABLE `',p_table,'` ADD CONSTRAINT `',p_name,'` ',p_definition);
    PREPARE crossdo_ddl FROM @crossdo_ddl_002;
    EXECUTE crossdo_ddl;
    DEALLOCATE PREPARE crossdo_ddl;
  END IF;
END$$
CREATE PROCEDURE crossdo_add_column_002(IN p_table VARCHAR(64), IN p_name VARCHAR(64), IN p_definition TEXT)
BEGIN
  IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema=DATABASE() AND table_name=p_table AND column_name=p_name) THEN
    SET @crossdo_ddl_002=CONCAT('ALTER TABLE `',p_table,'` ADD COLUMN `',p_name,'` ',p_definition);
    PREPARE crossdo_ddl FROM @crossdo_ddl_002;
    EXECUTE crossdo_ddl;
    DEALLOCATE PREPARE crossdo_ddl;
  END IF;
END$$
CREATE PROCEDURE crossdo_migrate_002()
BEGIN
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
  BEGIN
    ROLLBACK;
    RESIGNAL;
  END;
  IF NOT EXISTS (SELECT 1 FROM schema_migration WHERE version='002_schema_integrity') THEN
    ALTER TABLE `user` MODIFY COLUMN `user_permission` int NOT NULL, MODIFY COLUMN `real_name_authentication` tinyint(1) NOT NULL DEFAULT 0, MODIFY COLUMN `user_status` tinyint NOT NULL DEFAULT 1, MODIFY COLUMN `existing_projects` tinyint(1) NOT NULL DEFAULT 0;

    ALTER TABLE `project` MODIFY COLUMN `project_status` tinyint NOT NULL DEFAULT 1, MODIFY COLUMN `donation_pool_limit` DECIMAL(10,2) NOT NULL DEFAULT 0.00, MODIFY COLUMN `current_donations_received` DECIMAL(10,2) NOT NULL DEFAULT 0.00, MODIFY COLUMN `current_used_donations` DECIMAL(10,2) NOT NULL DEFAULT 0.00;

    CALL crossdo_add_column_002('project','submission_no','INT UNSIGNED NOT NULL DEFAULT 0');

    CALL crossdo_add_column_002('project','active_owner_id','CHAR(36) GENERATED ALWAYS AS (IF(project_status <> 13,RTRIM(user_id),NULL)) STORED');

    CALL crossdo_add_index_002('user','idx_user_review_queue','KEY `idx_user_review_queue` (`user_permission`,`user_status`,`user_id`)');

    CALL crossdo_add_index_002('user','idx_user_current_project','KEY `idx_user_current_project` (`project_id`,`user_id`)');

    CALL crossdo_add_index_002('project','uq_project_owner_reference','UNIQUE KEY `uq_project_owner_reference` (`project_id`,`user_id`)');

    CALL crossdo_add_index_002('project','uq_project_active_owner','UNIQUE KEY `uq_project_active_owner` (`active_owner_id`)');

    CALL crossdo_add_index_002('project','idx_project_reviewer_queue','KEY `idx_project_reviewer_queue` (`current_volunteer`,`project_status`,`project_create_time`,`project_id`)');

    CALL crossdo_add_index_002('patient_application','idx_application_submitted','KEY `idx_application_submitted` (`submitted_at`,`user_id`)');

    CALL crossdo_add_index_002('patient_application','idx_application_reviewer_queue','KEY `idx_application_reviewer_queue` (`current_volunteer`,`submitted_at`,`user_id`)');

    CALL crossdo_add_constraint_002('user','ck_user_status','CHECK (user_status BETWEEN 0 AND 13)');

    CALL crossdo_add_constraint_002('user','ck_user_verified','CHECK (real_name_authentication IN (0,1))');

    CALL crossdo_add_constraint_002('user','ck_user_project_flag','CHECK (existing_projects IN (0,1) AND existing_projects = (project_id IS NOT NULL))');

    CALL crossdo_add_constraint_002('user','ck_user_total_donations','CHECK (total_donation_amount IS NULL OR total_donation_amount >= 0)');

    CALL crossdo_add_constraint_002('project','ck_project_status','CHECK (project_status BETWEEN 0 AND 13)');

    CALL crossdo_add_constraint_002('project','ck_project_amounts','CHECK (donation_pool_limit >= 0 AND current_donations_received >= 0 AND current_used_donations >= 0 AND current_used_donations <= current_donations_received)');

    CALL crossdo_add_constraint_002('project','ck_project_submitted_target','CHECK (project_status NOT IN (0,2,3,6,7) OR donation_pool_limit > 0)');

    CALL crossdo_add_constraint_002('project','ck_project_assigned_flag','CHECK (question_is_volunteer_assigned IS NULL OR question_is_volunteer_assigned IN (0,1))');

    CALL crossdo_add_constraint_002('project','ck_project_history','CHECK (CASE WHEN verification_record IS NULL OR verification_record = '''' THEN TRUE WHEN JSON_VALID(verification_record) THEN JSON_TYPE(verification_record) = ''ARRAY'' ELSE FALSE END = 1)');

    CALL crossdo_add_constraint_002('project','ck_project_previous','CHECK (CASE WHEN previous_volunteers IS NULL OR previous_volunteers = '''' THEN TRUE WHEN JSON_VALID(previous_volunteers) THEN JSON_TYPE(previous_volunteers) = ''ARRAY'' ELSE FALSE END = 1)');

    CALL crossdo_add_constraint_002('project','ck_project_pathography','CHECK (CASE WHEN pathography IS NULL OR pathography = '''' THEN TRUE WHEN JSON_VALID(pathography) THEN JSON_TYPE(pathography) = ''ARRAY'' ELSE FALSE END = 1)');

    CALL crossdo_add_constraint_002('patient_application','ck_application_materials','CHECK (CASE WHEN JSON_VALID(materials) THEN JSON_TYPE(materials) = ''ARRAY'' ELSE FALSE END = 1)');

    CALL crossdo_add_constraint_002('patient_application','ck_application_history','CHECK (CASE WHEN verification_record IS NULL OR verification_record = '''' THEN TRUE WHEN JSON_VALID(verification_record) THEN JSON_TYPE(verification_record) = ''ARRAY'' ELSE FALSE END = 1)');

    CALL crossdo_add_constraint_002('patient_application','ck_application_previous','CHECK (CASE WHEN previous_volunteers IS NULL OR previous_volunteers = '''' THEN TRUE WHEN JSON_VALID(previous_volunteers) THEN JSON_TYPE(previous_volunteers) = ''ARRAY'' ELSE FALSE END = 1)');

    CALL crossdo_add_constraint_002('user','fk_user_permission','FOREIGN KEY (`user_permission`) REFERENCES `permission_group` (`id`) ON DELETE RESTRICT ON UPDATE RESTRICT');

    CALL crossdo_add_constraint_002('project','fk_project_owner','FOREIGN KEY (`user_id`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT');

    CALL crossdo_add_constraint_002('project','fk_project_reviewer','FOREIGN KEY (`current_volunteer`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT');

    CALL crossdo_add_constraint_002('patient_application','fk_application_user','FOREIGN KEY (`user_id`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT');

    CALL crossdo_add_constraint_002('patient_application','fk_application_reviewer','FOREIGN KEY (`current_volunteer`) REFERENCES `user` (`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT');

    CALL crossdo_add_constraint_002('user','fk_user_current_project','FOREIGN KEY (`project_id`,`user_id`) REFERENCES `project` (`project_id`,`user_id`) ON DELETE RESTRICT ON UPDATE RESTRICT');

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

    -- An interrupted migration may have created the tables. Runtime writes before
    -- the completion marker would make legacy sequence assignment ambiguous: stop.
    IF EXISTS (SELECT 1 FROM application_review_event WHERE legacy_import=0 LIMIT 1)
       OR EXISTS (SELECT 1 FROM project_review_event WHERE legacy_import=0 LIMIT 1)
       OR EXISTS (SELECT 1 FROM project_pathography_entry WHERE submission_no<>0 LIMIT 1) THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Migration 002 has unmarked runtime events; stop writers and reconcile before retrying';
    END IF;
    START TRANSACTION;

    -- Keep every original JSON value, including unknown historical shapes. Optional
    -- indexed fields are populated only when safely interpretable; no data is discarded.
    INSERT INTO application_review_event (user_id,sequence_no,actor_id,decision,recorded_at,record,legacy_import)
    SELECT v.user_id,v.sequence_no,u.user_id,
           CASE WHEN JSON_TYPE(JSON_EXTRACT(v.record,'$.decision'))='STRING' AND CHAR_LENGTH(v.event_decision)<=20
                THEN v.event_decision ELSE NULL END,
           CASE WHEN v.event_time REGEXP '^[1-9][0-9]{3}-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01]) ([01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]$'
             THEN CASE WHEN CAST(SUBSTRING(v.event_time,9,2) AS UNSIGNED) <= DAY(LAST_DAY(CONCAT(LEFT(v.event_time,7),'-01')))
                       THEN CAST(v.event_time AS DATETIME) ELSE NULL END
             ELSE NULL END,
           v.record,1
    FROM (
      SELECT r.*,JSON_UNQUOTE(JSON_EXTRACT(r.record,'$.actor_id')) AS event_actor,
             JSON_UNQUOTE(JSON_EXTRACT(r.record,'$.decision')) AS event_decision,
             JSON_UNQUOTE(JSON_EXTRACT(r.record,'$.time')) AS event_time
      FROM (
        SELECT p.user_id,j.sequence_no,JSON_EXTRACT(p.verification_record,CONCAT('$[',j.sequence_no-1,']')) AS record
        FROM patient_application p
        JOIN JSON_TABLE(COALESCE(NULLIF(p.verification_record,''),'[]'), '$[*]' COLUMNS(sequence_no FOR ORDINALITY)) j ON TRUE
      ) r
    ) v
    LEFT JOIN `user` u ON u.user_id=v.event_actor
    WHERE NOT EXISTS (SELECT 1 FROM application_review_event e WHERE e.user_id=v.user_id AND e.sequence_no=v.sequence_no);

    -- Keep every original JSON value, including unknown historical shapes. Optional
    -- indexed fields are populated only when safely interpretable; no data is discarded.
    INSERT INTO project_review_event (project_id,sequence_no,actor_id,decision,recorded_at,record,legacy_import)
    SELECT v.project_id,v.sequence_no,u.user_id,
           CASE WHEN JSON_TYPE(JSON_EXTRACT(v.record,'$.decision'))='STRING' AND CHAR_LENGTH(v.event_decision)<=20
                THEN v.event_decision ELSE NULL END,
           CASE WHEN v.event_time REGEXP '^[1-9][0-9]{3}-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01]) ([01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]$'
             THEN CASE WHEN CAST(SUBSTRING(v.event_time,9,2) AS UNSIGNED) <= DAY(LAST_DAY(CONCAT(LEFT(v.event_time,7),'-01')))
                       THEN CAST(v.event_time AS DATETIME) ELSE NULL END
             ELSE NULL END,
           v.record,1
    FROM (
      SELECT r.*,JSON_UNQUOTE(JSON_EXTRACT(r.record,'$.actor_id')) AS event_actor,
             JSON_UNQUOTE(JSON_EXTRACT(r.record,'$.decision')) AS event_decision,
             JSON_UNQUOTE(JSON_EXTRACT(r.record,'$.time')) AS event_time
      FROM (
        SELECT p.project_id,j.sequence_no,JSON_EXTRACT(p.verification_record,CONCAT('$[',j.sequence_no-1,']')) AS record
        FROM project p
        JOIN JSON_TABLE(COALESCE(NULLIF(p.verification_record,''),'[]'), '$[*]' COLUMNS(sequence_no FOR ORDINALITY)) j ON TRUE
      ) r
    ) v
    LEFT JOIN `user` u ON u.user_id=v.event_actor
    WHERE NOT EXISTS (SELECT 1 FROM project_review_event e WHERE e.project_id=v.project_id AND e.sequence_no=v.sequence_no);

    INSERT INTO project_pathography_entry(project_id,submission_no,sequence_no,record)
    SELECT p.project_id,0,j.sequence_no,JSON_EXTRACT(p.pathography,CONCAT('$[',j.sequence_no-1,']'))
    FROM project p
    JOIN JSON_TABLE(COALESCE(NULLIF(p.pathography,''),'[]'), '$[*]' COLUMNS(sequence_no FOR ORDINALITY)) j ON TRUE
    WHERE NOT EXISTS (SELECT 1 FROM project_pathography_entry e WHERE e.project_id=p.project_id AND e.submission_no=0 AND e.sequence_no=j.sequence_no);

    INSERT INTO schema_migration(version,applied_at) VALUES ('002_schema_integrity',UTC_TIMESTAMP());
    COMMIT;
  END IF;
END$$
DELIMITER ;
CALL crossdo_migrate_002();
DROP PROCEDURE crossdo_migrate_002;
DROP PROCEDURE crossdo_add_column_002;
DROP PROCEDURE crossdo_add_constraint_002;
DROP PROCEDURE crossdo_add_index_002;
