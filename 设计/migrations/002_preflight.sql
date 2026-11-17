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
