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
