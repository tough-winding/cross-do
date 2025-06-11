-- MySQL dump 10.13  Distrib 8.0.21, for Linux (x86_64)
--
-- Host: 127.0.0.1    Database: cross-do
-- ------------------------------------------------------
-- Server version       8.0.21

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;
SET @MYSQLDUMP_TEMP_LOG_BIN = @@SESSION.SQL_LOG_BIN;
SET @@SESSION.SQL_LOG_BIN= 0;

--
-- GTID state at the beginning of the backup 
--

SET @@GLOBAL.GTID_PURGED=/*!80000 '+'*/ '1ff2aaac-8ced-11ee-a824-6c92bf67fc44:1-2336441';

--
-- Table structure for table `dispute`
--

DROP TABLE IF EXISTS `dispute`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `dispute` (
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
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `dispute`
--

LOCK TABLES `dispute` WRITE;
/*!40000 ALTER TABLE `dispute` DISABLE KEYS */;
INSERT INTO `dispute` VALUES ('d9ac8c21-1d3a-4d3c-a83a-29f76360f68d',903,'2b3c4d5e-6f7a-8b9c-0d1e-f11g2h3i4j5k','a5a6a73e-7f79-4e9c-891b-6b0f2db3e3ab',1,'[{\"time\":\"2023-01-03 11:00:00\",\"content\":\"经去医院排查，与相关医师和护士沟通后发现并无此人，该项目疑似骗捐项目，请冻结该项目。并自身审计下，为什么会发生这种事情，是否资料审计不够完善。详细证据请看图。\",\"images\":\"http://10.0.0.15:9000/dispute/d9ac8c21-1d3a-4d3c-a83a-29f76360f68d/20230103110000.jpg\"}]');
/*!40000 ALTER TABLE `dispute` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `permission_group`
--

DROP TABLE IF EXISTS `permission_group`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `permission_group` (
  `id` int NOT NULL,
  `group_name` varchar(20) DEFAULT NULL,
  `group_illustrate` varchar(100) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `permission_group`
--

LOCK TABLES `permission_group` WRITE;
/*!40000 ALTER TABLE `permission_group` DISABLE KEYS */;
INSERT INTO `permission_group` VALUES (1,'admin','保留，暂不规划。'),(2,'donor','捐助者；可审查已捐助项目。举报项目/志愿者。'),(3,'sufferer','患者；可发起项目，修改自身项目部分信息，投诉志愿者。'),(5,'volunteer','志愿者：可被分配/主动接取任务。可参与投诉/举报审计。可举报项目/患者。发起平台提议。'),(6,'long term donor','长期捐助者；可审查已捐助项目。举报项目/志愿者。可发起平台提议。可参与扩大投诉/举报审计。'),(7,'senior volunteer','资深志愿者；可被分配/主动接取任务。可参与投诉/举报审计。可举报项目/患者。发起平台提议。参与专项调查。');
/*!40000 ALTER TABLE `permission_group` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `project`
--

DROP TABLE IF EXISTS `project`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `project` (
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
  `donation_pool_limit` mediumint DEFAULT NULL,
  `current_donations_received` mediumint DEFAULT NULL,
  `current_used_donations` mediumint DEFAULT NULL,
  `last_update_time` datetime DEFAULT NULL,
  `pathography` text,
  PRIMARY KEY (`project_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `project`
--

LOCK TABLES `project` WRITE;
/*!40000 ALTER TABLE `project` DISABLE KEYS */;
INSERT INTO `project` VALUES ('a5a6a73e-7f79-4e9c-891b-6b0f2db3e3ab','1a2b3c4d-5e6f-7a8b-9c0d-ef11g2h3i4j5','唐百',28,'唐铁嘴',8,'[{\"患者\":{\"唐百\":\"请志愿者看下这次的资料合格了吗？\"},\"志愿者\":{\"释迦摩尼\":\"您的资料还存在以下问题：病历不完整、没有医生签字证明。请补充好之后再提交一下。\"}},{\"患者\":{\"唐百\":\"我又调整了下，您再看下呢？\"},\"释迦摩尼\":{\"bceac\":\"这次可以了，给您通过了。\"}}]','e7b15d41-df6c-4f2a-a4d9-2c0e94380e2b','[\"e7b15d41-df6c-4f2a-a4d9-2c0e94380e2b\"]',1,'d9ac8c21-1d3a-4d3c-a83a-29f76360f68d','2023-01-02 15:00:00',10000,5000,2500,'2023-01-02 16:00:00','[{\"time\":\"2023-01-02 15:00:00\",\"content\":\"刚刚确诊为***，心情很是沉重，医生说需要******来治疗\",\"images\":\"http://10.0.0.15:9000/pathography/a5a6a73e-7f79-4e9c-891b-6b0f2db3e3ab/20200102150000-1.jpg;http://10.0.0.15:9000/pathography/a5a6a73e-7f79-4e9c-891b-6b0f2db3e3ab/20200102150000-2.jpg\"},{\"time\":\"2020-01-02 16:00:00\",\"content\":\"医生刚刚又说，可能还有其他问题，唉。\",\"images\":\"\"}]');
/*!40000 ALTER TABLE `project` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `user`
--

DROP TABLE IF EXISTS `user`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `user` (
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
  `total_donation_amount` mediumint DEFAULT NULL,
  PRIMARY KEY (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `user`
--

LOCK TABLES `user` WRITE;
/*!40000 ALTER TABLE `user` DISABLE KEYS */;
INSERT INTO `user` VALUES ('1a2b3c4d-5e6f-7a8b-9c0d-ef11g2h3i4j5',2,'zhangsan','hashed_password','http://10.0.0.15:9000/default_resources/profile_picture1.png','张扇封','210922124503227651','圣者渡人',1,0,0,'','c44b8860-43f5-4d8a-a3e9-1e6d24fd4f7a','13800138000',30,'2023-01-01 10:00:00','2023-01-02 08:00:00',''),('2b3c4d5e-6f7a-8b9c-0d1e-f11g2h3i4j5k',3,'唐百','another_hashed_password','http://10.0.0.15:9000/profile_picture/9Jk3pWv5RfX2lQ7t1YbZ.png','唐铁嘴','110101188503309953','妙算圣手',1,0,1,'a5a6a73e-7f79-4e9c-891b-6b0f2db3e3ab','f2b1760c-6c13-43d6-96a9-81f0c8e13d68','13900139000',28,'2023-01-02 11:00:00','2023-01-03 09:00:00','e6780d85-8ca0-4a95-ae38-3a02a7f1ebf7'),('e7b15d41-df6c-4f2a-a4d9-2c0e94380e2b',1,'释迦摩尼','hashed_password','http://10.0.0.15:9000/profile_picture/X8qP9bKw3N1vZ2L6R4f7T.png','乔达摩·悉达多','310101199001012345','人生八苦：生老病死 爱别离 怨憎会 求不得 五阴炽盛。',1,0,0,'','d9f53b85-cbd4-4e32-9a1a-68157c2e8d75','15140279010',80,'2023-01-01 10:00:00','2023-01-03 09:00:00','');
/*!40000 ALTER TABLE `user` ENABLE KEYS */;
UNLOCK TABLES;
SET @@SESSION.SQL_LOG_BIN = @MYSQLDUMP_TEMP_LOG_BIN;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;


--
-- Table structure for table `ledger_investigations`
--

DROP TABLE IF EXISTS `ledger_investigations`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `ledger_investigations` (
  `investigation_id` CHAR(36)  NOT NULL,            -- 主键：流水问题人工介入调查唯一ID
  `ledger_id` CHAR(36)  DEFAULT NULL,               -- 资金流水唯一ID
  `user_id` CHAR(36)  DEFAULT NULL,                 -- 介入发起用户UUID
  `user_note` text  DEFAULT NULL,                   -- 用户反馈问题记录
  `current_operation` char(36) DEFAULT NULL,        -- 当前负责运维ID
  `conclusion_note` text DEFAULT NULL,              -- 当前事件结论
  PRIMARY KEY (`investigation_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `ledger_investigations`
--


--
-- Table structure for table `donation_ledger`
--

DROP TABLE IF EXISTS `donation_ledger`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `donation_ledger` (
  `ledger_id` CHAR(36)  NOT NULL,                   -- 主键：资金流水唯一ID
  `user_id` CHAR(36)  DEFAULT NULL,                 -- 发起人（捐赠者/患者/操作者）
  `sufferer_user_id` CHAR(36)  DEFAULT NULL,        -- 项目受助者ID
  `project_id` CHAR(36)  DEFAULT NULL,              -- 所属项目（含特殊池）
  `donor_user_name` VARCHAR(24) DEFAULT NULL,       -- 捐赠者用户名
  `sufferer_real_name` VARCHAR(20) DEFAULT NULL,    -- 患者实名（仅用于存档）
  `sufferer_user_name` VARCHAR(24) DEFAULT NULL,    -- 患者用户名（辅助显示）
  `amount` MEDIUMINT NOT NULL,                      -- 金额，正负表示入账/出账
  `transaction_time` DATETIME  NOT NULL,            -- 发生时间（统一命名）
  `note` VARCHAR(200) DEFAULT NULL,                 -- 备注
  `payment_method` TINYINT   DEFAULT NULL,          -- 支付方式（0：支付宝 1：微信 2：银行卡）
  `method_id` CHAR(36)  DEFAULT NULL,               -- 支付平台返回的ID，如微信单号
  `transaction_type` TINYINT   NOT NULL,            -- 类型（0：捐赠 1：退款待选择 2：退款 3：转应急池 4：患者使用 5：应急使用 6：平台使用）
  `status` TINYINT   DEFAULT 1,                     -- 状态（0：成功 1：等待支付 2：支付失败 3：处理中 4：处理失败 5：退款中 6：退款失败 7：退款完成 8：发款中 9：发款失败 10：发起人工介入 11：人工介入中 12：人工介入完成）
  `investigation_id` CHAR(36)  DEFAULT NULL,        -- 人工介入调查ID
  PRIMARY KEY (`ledger_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `donation_ledger`
--


/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;