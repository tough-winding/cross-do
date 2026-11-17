# 部署与配置

本文提供单台 Linux 服务器部署方法：Nginx 提供 HTTPS，Java 同时提供前端静态页面和 `/api`，IAM 负责服务认证，C DBOP 负责数据库事务；MySQL 和 Redis 仅供内部服务使用。所有命令从仓库根目录执行，除注明的系统操作外，不依赖开发工作区的 `/workspace` 或 `/tmp/crossdo-review-stack`。

首次部署先保持短信和支付为 `disabled`。未配置短信时无法注册、找回密码或换号；配置真实短信网关后才能向实际用户开放注册。供应商未确定时，可在隔离本机环境使用 `development` 演示，验证码会直接返回，付款也仅模拟，不能用作公开收款系统。

## 1. 系统与构建依赖

建议 Ubuntu 22.04/24.04 或 Debian，使用 MySQL 8.0.16+。MariaDB 需 11.8+；不要直接使用较老系统自带的 MariaDB 10.x，它不支持本项目使用的排序规则。数据库安装包和服务名称根据实际发行版选择，以下以 MySQL 的 `mysql.service` 和 Redis 的 `redis-server.service` 为例。

构建需要 JDK 11、Maven 3.8+、Node.js 20.19+ 或 22.12+、npm、Python 3，以及 C 开发库。通过发行版或官方仓库安装 JDK 11 与 Node.js，先核对实际版本：

```bash
java -version
mvn -version
node --version
npm --version
```

Ubuntu/Debian 的 C 依赖、Redis 和反向代理：

```bash
sudo apt-get update
sudo apt-get install build-essential git pkg-config python3 maven \
  libevent-dev libjansson-dev libyaml-dev uuid-dev libhiredis-dev \
  libmariadb-dev libmariadb-dev-compat libcurl4-openssl-dev libssl-dev \
  redis-server nginx openssl
```

Ubuntu 22.04/24.04 可通过 `sudo apt-get install mysql-server mysql-client` 安装 MySQL 8；安装后用 `mysql --version` 核对版本。Debian 需先配置 MySQL 官方仓库安装 MySQL 8，或使用 MariaDB 11.8+ 官方仓库，不能将 `default-mysql-server` 的实际版本视为必然满足要求。

还需要 `libzlog-dev`。发行版提供时执行 `sudo apt-get install libzlog-dev`。没有该包时，可以单独构建 zlog；下面使用已固定的上游 1.2.18 版本，不修改项目依赖：

```bash
git clone --branch 1.2.18 --depth 1 https://github.com/HardySimpson/zlog.git /tmp/crossdo-zlog-build
make -C /tmp/crossdo-zlog-build
sudo make -C /tmp/crossdo-zlog-build install
sudo ldconfig
```

JDK/Maven/Node 只需在构建机提供，运行机需 Java 11 和 C 动态链接库。跨机器复制 C 程序时使用相同架构及兼容发行版；检查 `ldd build/dbop`、`ldd build/iam` 不出现 `not found`。

获取仓库后构建完整前后端；已有工作副本直接进入其目录：

```bash
git clone https://github.com/tough-winding/cross-do.git
cd cross-do
make c
make package
```

`make package` 先安装前端锁定依赖并构建，再运行 Java 测试及打包。产物为 `build/dbop`、`build/iam`、`java/org.crossdo/target/crossdo-0.0.1-SNAPSHOT.jar`。JAR 已包含前端，生产无需运行 Vite 或 Node 服务。默认使用 MariaDB 客户端库连接 MySQL；使用 MySQL 客户端开发库时执行 `make c MYSQL_LIB=-lmysqlclient`。

## 2. 发布目录和服务用户

创建专用系统账号，并将配置、程序、上传和日志分开存放。发布编号可换为自己的版本号；以下以 `release-001` 为例。

```bash
sudo useradd --system --home-dir /var/lib/crossdo --shell /usr/sbin/nologin crossdo
sudo install -d -o root -g crossdo -m 0750 /etc/crossdo /etc/crossdo/iam /etc/crossdo/iam/config
sudo install -d -o crossdo -g crossdo -m 0700 /var/lib/crossdo /var/lib/crossdo/uploads /var/log/crossdo
sudo install -d -o root -g root -m 0755 /opt/crossdo/releases/release-001/bin /opt/crossdo/releases/release-001/app
sudo install -o root -g root -m 0755 build/dbop /opt/crossdo/releases/release-001/bin/dbop
sudo install -o root -g root -m 0755 build/iam /opt/crossdo/releases/release-001/bin/iam
sudo install -o root -g root -m 0644 java/org.crossdo/target/crossdo-0.0.1-SNAPSHOT.jar /opt/crossdo/releases/release-001/app/crossdo.jar
sudo ln -s /opt/crossdo/releases/release-001 /opt/crossdo/current
```

若用户或 `current` 已存在，沿用现有账号；升级时按后面的停服/切换步骤操作，不能直接覆盖运行中的 JAR。

## 3. MySQL 初始化或升级

MySQL 监听 `127.0.0.1:3306`，启用 InnoDB；数据库时间由业务连接设为 UTC。结构初始化/迁移使用数据库管理员，运行时使用权限较少的 `crossdo_app`。不要把数据库管理员密码交给 Java 或 DBOP。

先复制数据库引导模板到仅 root 可读的位置，用 `sudoedit` 将 `CHANGE_ME_DATABASE_PASSWORD` 换为独立随机密码，建议使用 `openssl rand -hex 32` 生成，再从密码管理器写入配置。模板中的随机密码应是普通字符串，不要写 `$变量`；C YAML 不会做环境变量替换。

```bash
sudo install -o root -g root -m 0600 deploy/database.sql.example /etc/crossdo/database.sql
sudoedit /etc/crossdo/database.sql
sudo sh -c 'mysql < /etc/crossdo/database.sql'
```

这一步创建 `crossdo` 库和 `'crossdo_app'@'127.0.0.1'`，运行权限为该库的 SELECT/INSERT/UPDATE。已存在账号时由数据库管理员核对并调整权限，不要为了重跑删除账号。DBOP 启动时仍准备部分历史语句，因此需保留完整 schema 中的旧表，不能只导入新增模块。

**新空库**导入完整结构：

```bash
sudo mysql crossdo < 设计/schema.sql
```

**已有业务库**禁止用空库脚本覆盖。先停止全部写入方并备份，按顺序升级；`002_preflight.sql` 是只读检查，但依赖 001 已创建的申请/事件表，因此必须先执行 001，再在 002 前运行预检：

```bash
sudo mysql crossdo < 设计/migrations/001_review_workflow.sql
sudo mysql crossdo < 设计/migrations/002_preflight.sql
sudo mysql crossdo < 设计/migrations/002_schema_integrity.sql
sudo mysql crossdo < 设计/migrations/003_operations.sql
sudo mysql crossdo < 设计/migrations/004_account_security.sql
sudo mysql crossdo < 设计/migrations/005_files.sql
sudo mysql crossdo < 设计/migrations/006_finance.sql
```

不能用 `mysql --force` 忽略错误。迁移可重复执行，但 MySQL DDL 会隐式提交，整次升级不具备事务回滚；异常时保持停写，按异常类别核实真实数据后重跑。不要使用历史 `设计/测试数据.sql` 初始化。结构和历史资金的处理见[数据库优化说明](设计/数据库优化说明.md)。

配置 Java 的服务身份。编辑另一个模板，将 `CHANGE_ME_WEB_SERVICE_PASSWORD` 换为与数据库密码不同的随机值：

```bash
sudo install -o root -g root -m 0600 deploy/service-credentials.sql.example /etc/crossdo/service-credentials.sql
sudoedit /etc/crossdo/service-credentials.sql
sudo sh -c 'mysql crossdo < /etc/crossdo/service-credentials.sql'
sudo mysql crossdo -e 'SELECT version FROM schema_migration ORDER BY version;'
```

空库脚本完成后应看到 002 至 006 对应的标记；按迁移升级的库还包含 001 标记。`service` 中的 `CROSSDO_WEB` 密码必须与稍后 Java 的 `CROSSDO_SERVICE_PASSWORD` 一致；既有记录由管理员显式更新，不要误覆盖其他服务。IAM 启动后自行把 `IAM_SERVICE` 令牌写入 Redis，不需要默认服务密码或手工复制 token。

上面 `sudo mysql` 适用于本机管理员 socket 登录。远程数据库或其他认证方式使用运维独有的 MySQL option file / `mysql_config_editor` login path；不要把口令写进命令行。C DBOP 当前配置未提供数据库 TLS 证书参数，本文采用本机数据库拓扑。

## 4. Redis 与 C 服务配置

Redis 监听 `127.0.0.1:6379`，保留 `protected-mode yes`，为默认用户设置独立随机 `requirepass`。Java、IAM、DBOP 必须使用同一个 Redis 实例、同一密码和 DB 0。两个 C 服务当前仅支持 `AUTH password`，不支持 Redis ACL 用户名、TLS 或切换 logical database；不要单独设置 Java `SPRING_REDIS_DATABASE`。不要与不受信任的应用共享该实例。

在发行版 `/etc/redis/redis.conf` 中配置 `bind 127.0.0.1`、`protected-mode yes`、`requirepass <实际随机密码>`，再 `sudo systemctl restart redis-server`。持久化按运维策略启用 AOF；Redis 丢失会让登录、验证码和服务令牌失效，MySQL 中的资金流水不会因此丢失。Redis 重启后应重启 IAM，再重启 Java，重新生成服务令牌；用户需要重新登录。

复制 C 配置及日志模板：

```bash
sudo install -o root -g crossdo -m 0640 deploy/dbop.yaml.example /etc/crossdo/dbop.yaml
sudo install -o root -g crossdo -m 0640 deploy/iam.yaml.example /etc/crossdo/iam/config/iam_config.yaml
sudo install -o root -g crossdo -m 0640 deploy/dbop-zlog.conf /etc/crossdo/dbop-zlog.conf
sudo install -o root -g crossdo -m 0640 deploy/iam-zlog.conf /etc/crossdo/iam-zlog.conf
sudoedit /etc/crossdo/dbop.yaml /etc/crossdo/iam/config/iam_config.yaml
```

替换两份 YAML 中的 Redis 密码，并将 DBOP 的数据库密码与前面 SQL 中的值对齐。所有配置都是平坦字符串，端口和连接池大小也加引号。`database_pool_size`、`redis_pool_size` 先用模板的 3；并发增大前观察实际数据库和 Redis 连接数。

IAM 固定从当前目录读取 `config/iam_config.yaml`；模板 systemd 已将 WorkingDirectory 设为 `/etc/crossdo/iam`。DBOP 支持命令行传入配置绝对路径。C 配置里的 `server_ip` 不决定监听地址；两个服务都由 `CROSSDO_BIND_ADDRESS=127.0.0.1` 限制在本机。

## 5. Java、短信和支付配置

```bash
sudo install -o root -g root -m 0600 deploy/web.env.example /etc/crossdo/web.env
sudoedit /etc/crossdo/web.env
```

`web.env` 是 systemd EnvironmentFile，使用 `NAME=value`，不加 `export`，不执行 shell 命令或变量展开。主要配置如下：

| 配置 | 本部署示例 | 含义 |
| --- | --- | --- |
| `SERVER_ADDRESS` / `SERVER_PORT` | `127.0.0.1` / `8080` | 仅本机 Nginx 访问 Java |
| `SPRING_PROFILES_ACTIVE` | `prod` | 生产错误信息设置 |
| `SPRING_REDIS_*` | 本机地址、6379、实际密码 | 和两个 C 服务同实例/DB 0 |
| `CROSSDO_DBOP_URL` / `CROSSDO_IAM_URL` | `http://127.0.0.1:1900` / `http://127.0.0.1:10089` | 内部服务 HTTP 地址 |
| `CROSSDO_SERVICE_NAME` / `CROSSDO_SERVICE_PASSWORD` | `CROSSDO_WEB` / 实际密码 | 与 MySQL service 记录一致 |
| `CROSSDO_SESSION_TTL` | `PT2H` | ISO-8601 固定会话有效期 |
| `CROSSDO_UPLOAD_DIR` | `/var/lib/crossdo/uploads` | 持久私有文件目录，与元数据一起备份 |
| `CROSSDO_SMS_MODE` | `disabled` | `disabled` / `development` / `gateway` |
| `CROSSDO_SMS_GATEWAY_URL` / `CROSSDO_SMS_GATEWAY_KEY` | 配置真实网关时填写 | 完整 HTTPS POST 地址及 Bearer 密钥 |
| `CROSSDO_PAYMENT_MODE` | `disabled` | `disabled` / `development` / `gateway` |
| `CROSSDO_PAYMENT_GATEWAY_URL` / `CROSSDO_PAYMENT_GATEWAY_KEY` | 配置真实网关时填写 | HTTPS 基地址及 Bearer 密钥 |
| `CROSSDO_PAYMENT_WEBHOOK_SECRET` | 配置真实网关时填写 | 至少 32 字符的独立 HMAC 密钥 |

`prod` 不会自动覆盖错误的 `development` 模式，部署前必须核对两个 MODE。开发模式没有真实短信或资金，但平台仍记录模拟订单；模拟过资金的项目不能改为真实网关项目，即使余额已清零。

真实供应商通过约定适配器接入：短信 POST 手机号、用途、验证码、有效期和幂等引用；支付基地址提供 `/payments`、`/refunds`、`/disbursements`；支付结果通知至 `https://你的域名/api/payments/callback`，对原始 JSON 和时间戳做 HMAC-SHA256 签名。必须遵守重试、金额字符串和终态规则，完整请求/响应及签名示例见[运营与服务接入](设计/运营与服务接入.md)。配置 URL 和密钥不会自动完成某家供应商的适配，需要该网关按合同实现并联调。

模板还启用 Tomcat 原生转发头处理，并把可信代理限定为 loopback。它与下面 Nginx 覆盖 `X-Forwarded-For`、Java 仅监听 loopback 配套使用，使验证码 IP 限流使用访问者地址。改变代理拓扑时同步调整可信代理；若关闭转发头处理，代理后的访问者会共享代理 IP 的限额。

上传文件不能放在 Nginx 静态目录中。单文件限 10 MiB、每账号合计 500 MiB，Java multipart 总请求限 11 MiB；通过 `/api/files/{uuid}` 验证访问权限。多 Java 实例必须共享同一持久文件存储，此部署模板仅配置一个 Java 实例。

## 6. 启动和验证

确认实际配置中没有剩余占位符，以下命令只输出文件名：

```bash
sudo grep -rl CHANGE_ME /etc/crossdo
```

安装 systemd 单元和日志轮转规则：

```bash
sudo install -m 0644 deploy/systemd/crossdo-dbop.service /etc/systemd/system/crossdo-dbop.service
sudo install -m 0644 deploy/systemd/crossdo-iam.service /etc/systemd/system/crossdo-iam.service
sudo install -m 0644 deploy/systemd/crossdo-web.service /etc/systemd/system/crossdo-web.service
sudo install -m 0644 deploy/logrotate.conf /etc/logrotate.d/crossdo
sudo systemctl daemon-reload
sudo systemctl enable --now mysql redis-server
sudo systemctl enable --now crossdo-dbop crossdo-iam crossdo-web
sudo systemctl status crossdo-dbop crossdo-iam crossdo-web --no-pager
```

systemd `After` 管理进程启动顺序，不代表依赖已经健康；失败时配置中的 5 秒重启间隔会重试。先确认 MySQL/Redis，再依次检查 DBOP、IAM、Java。

```bash
curl --fail --silent http://127.0.0.1:8080/api/auth/settings
curl --fail --silent http://127.0.0.1:8080/api/payments/settings
curl --fail --silent --output /dev/null http://127.0.0.1:8080/
curl --silent --output /dev/null --write-out '%{http_code}\n' http://127.0.0.1:8080/api/me
sudo journalctl -u crossdo-web -n 100 --no-pager
sudo ss -lntp
```

设置接口应显示选择的 MODE；未登录 `/api/me` 应为 401。首页和设置接口成功只验证 Java 已启动，不能证明 MySQL/IAM 链路正常；配置短信后继续以专用账号完成注册/登录，再核对账户 API 和管理员页面。内部 1900、10089、3306、6379 及 Java 8080 应只监听 loopback，不直接开放到公网。

Java 日志进入 journal；C 日志在 `/var/log/crossdo/dbop.log` 和 `iam.log`。日志轮转采用 copytruncate，轮转瞬间可能有少量日志损失，业务审计和资金流水仍保存在 MySQL；按磁盘容量调整保留期，日志只授予运维人员访问。

## 7. HTTPS 与域名

将域名 A/AAAA 指向服务器，在网络防火墙开放 80/443。先用所选 ACME 客户端申请证书，再启用模板中的 HTTPS server；例如已安装 Certbot 时可暂停 Nginx 使用 standalone 签发，再恢复服务。避免在证书文件不存在时启用 HTTPS 模板。

```bash
sudo install -m 0644 deploy/nginx.conf.example /etc/nginx/sites-available/crossdo
sudoedit /etc/nginx/sites-available/crossdo
sudo ln -s /etc/nginx/sites-available/crossdo /etc/nginx/sites-enabled/crossdo
sudo nginx -t
sudo systemctl reload nginx
```

把模板里的 `example.com` 和证书路径全部替换。一个 Nginx 站点将 `/` 和 `/api` 都转发 Java，HashRouter 无需额外 SPA rewrite。`client_max_body_size 12m` 留出 multipart 开销；不能把 `/api/files` 改成磁盘 alias，也不要缓存带会话的 API。证书自动续期后配置 Nginx reload。

```bash
curl --fail --silent --output /dev/null https://你的域名/
curl --fail --silent https://你的域名/api/auth/settings
```

## 8. 首位管理员和验收

配置真实短信网关后，通过网页验证码注册一个由实际管理员控制的账号，记录其 UUID，再从仓库运行离线引导。先准备仅 root 可读的 option file。下列模板使用本文的本机 root socket 认证；若使用其他运维身份，编辑为对应账号及密码/TLS 选项，不要使用权限不足的运行账号。

```bash
sudo install -o root -g root -m 0600 deploy/admin-db.cnf.example /etc/crossdo/admin-db.cnf
sudoedit /etc/crossdo/admin-db.cnf
```

随后先预览，不写数据，替换为真实 UUID：

```bash
sudo python3 tools/bootstrap_admin.py \
  --defaults-file /etc/crossdo/admin-db.cnf --database crossdo \
  --user-id 11111111-1111-4111-8111-111111111111 \
  --note '实际操作人及授权工单编号'
```

核对目标后，用相同参数加 `--apply`；脚本只授权已存在且 `phone_verified=1` 的账号，不创建默认密码。授权会递增会话版本并写入 `admin_audit`，须重新登录。管理员随后可认证志愿者、转派任务、处理举报及资金/结项审批。数据库权限和审计说明见运营文档。

在隔离环境验收注册、患者资料上传与审核、项目审核、模拟付款/部分退款/用款/零余额结项、冻结与改密后旧会话失效。自动化回归见 README；测试会创建账号和记录，不能把测试脚本指向生产数据库。真实渠道上线前还须使用其测试环境验证签名回调、相同引用重试和供应商对账。

## 9. 升级、备份和故障处理

升级前先构建新的独立发布目录，保留旧程序；安排维护窗口并暂停外部回调投递或确保网关会重试。停止 Java、IAM、DBOP 后，在无写入期间备份 MySQL 和上传目录，例如：

```bash
sudo systemctl stop crossdo-web crossdo-iam crossdo-dbop
sudo install -d -o root -g root -m 0700 /var/backups/crossdo/release-001
sudo sh -c 'umask 077; mysqldump --single-transaction --routines --triggers crossdo > /var/backups/crossdo/release-001/database.sql'
sudo tar -C /var/lib/crossdo -czf /var/backups/crossdo/release-001/uploads.tar.gz uploads
```

执行本版本新增迁移，成功后把 `current` 切向已经安装完整的新发布目录，再依次启动三个服务。切换示例中的新目录须真实存在：

```bash
sudo ln -s /opt/crossdo/releases/release-002 /opt/crossdo/current.next
sudo mv -Tf /opt/crossdo/current.next /opt/crossdo/current
sudo systemctl start crossdo-dbop crossdo-iam crossdo-web
```

配置文件和密钥也应在受控位置加密备份，不能提交到 Git。定期在隔离环境验证数据库与文件联合恢复；元数据和文件应来自同一个停写备份窗口。代码可切回旧发布，但发生 schema 升级后，旧程序不一定兼容新数据库；保持停写并使用配套备份恢复方案，不要仅回滚二进制后开放旧写接口。恢复旧账前先核对支付供应商状态，避免丢失备份后已发生的真实款项。

| 现象 | 优先检查 |
| --- | --- |
| DBOP 启动即退出 | YAML 路径/平坦结构、数据库密码与权限、全部迁移、动态库、日志目录写权限 |
| IAM 启动失败/认证不可用 | 固定 WorkingDirectory、Redis 默认用户密码/DB 0、DBOP 端口、共享 Redis |
| Java 返回数据服务 503 | C 服务健康、CROSSDO_WEB 密码与 service 表、IAM/DBOP 地址；不要把临时网络超时当成写入失败自动新建订单 |
| 验证码 429 | 手机/用途冷却和小时限额、可信代理地址；不要在生产清空 Redis 绕过限流 |
| 支付被禁用 | 两个 MODE 的实际配置，网关 HTTPS/密钥，模拟资金项目不能切换真实模式 |
| 上传 413 或文件不可读 | Nginx/Java 大小限制、磁盘空间与目录权限、元数据/文件备份一致性、调用者访问资格 |
| 结项返回 409 | 待处理支付/退款/用款、余额/预留及流水对账；并发冲突用原幂等键和引用重新读取后重试 |

日常在管理员资金对账页核对流水、账户余额及缓存金额，保留所有异常记录。历史汇总不会自动变成新资金流水，不通过直接改余额、删流水或虚构成功订单消除差异。
