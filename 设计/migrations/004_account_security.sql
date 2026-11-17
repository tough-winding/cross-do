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
