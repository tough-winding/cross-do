# 渡舟 · CrossDo
浊水惊浪夜暮愁，
孤帆一叶复归州。
凭栏遥望灯明远，
愿予清风助渡舟。

## 部署与配置入口

完整部署步骤见 [DEPLOYMENT.md](DEPLOYMENT.md)，包含从空服务器构建、MySQL 初始化与升级、Redis 和服务凭据配置、systemd 自启动、Nginx HTTPS、检查与备份恢复。可复制的配置模板放在 [deploy/](deploy/)，短信/支付网关及运营流程见 [运营与服务接入](设计/运营与服务接入.md)。

## 开发进度

设计位于 `设计/整体流程图.png`、`设计/数据库设计.png`。当前已实现前后端主流程：

- 中文响应式前端：公益首页、项目广场、注册登录、患者申请、我的项目、志愿审核工作台及账户资料。
- 手机验证码注册、BCrypt 密码存储、Redis 登录会话、找回密码与换绑手机号；改密、换号和冻结会撤销全部旧登录。
- 患者提交材料，志愿者在任务池接单，审核通过、要求补正或标记联系不上；补正后可以重新提交。
- 已审核患者创建项目、提交病情和募款目标，志愿者接单审核；通过的项目进入公开项目列表，患者可追加病情记录。
- 接单和审核使用数据库事务与行锁，审核事件逐条追加到明细表，项目补正保留每次提交的金额及材料快照；公开项目隐藏实名与审核资料，单笔金额使用 `DECIMAL(10,2)`，累计资金使用 `DECIMAL(14,2)`。
- 管理员后台：志愿者认证、账号冻结/解冻、任务超时提示与转派、举报申诉处理、结项审核及操作审计。
- PNG/JPEG/PDF 材料上传、按申请/项目版本控制访问、站内通知及未读计数。
- 捐赠订单、部分退款、用款审批、公开资金使用记录和对账；余额预留、精确金额及幂等回调防止重复记账。

Java 提供面向用户的 API，通过 IAM 获取服务凭据并调用 C DBOP；MySQL 读写统一由 DBOP 执行。原来的 MyBatis 实体仍保留，默认运行不依赖 Java 数据源；`direct-db` profile 用于维护旧映射。

短信和支付按当前需求提供显式开发模式与可配置 HTTPS 网关接口，默认关闭；开发模式仅模拟，不产生真实短信或资金。实际渠道尚未选定，接入前按 [运营与服务接入](设计/运营与服务接入.md) 配置并联调。患者实名状态由志愿者材料审核设置。地域匹配、自动短信催办、外部实名核验仍未接入。

## 构建与启动

需要 JDK 11、Maven、MySQL 8.0.16+（需要强制执行 CHECK）、Redis，以及 C 编译器和 libevent、jansson、zlog、libyaml、uuid、hiredis、MySQL/MariaDB client、curl、OpenSSL 开发库。

```sh
make c
make package
```

C 产物为 `build/dbop` 与 `build/iam`。MySQL client 链接库默认 `-lmariadb`，使用 MySQL client 时执行 `make c MYSQL_LIB=-lmysqlclient`。云工作区已有工具时，可先执行 `source /workspace/.crossdo-tools/activate.sh`。

空数据库导入 `设计/schema.sql`；已有数据库备份并停止写入后，按编号依次执行 `设计/migrations/` 中 001 至 006 的迁移（`002_preflight.sql` 为只读预检），成功后再启动新版 DBOP。002 会先检查旧数据，遇到不一致时按类别报告并停止，不会自动清洗。MySQL DDL 会隐式提交，若中途失败，应保持停写、修正问题后重跑，不能把旧程序与新结构混用。详见 [数据库优化说明](设计/数据库优化说明.md)。旧的 `设计/测试数据.sql` 是历史导出，含破坏性初始化语句及不一致的样例，不应作为新版本初始化脚本。

仓库 `DBOP/config/db_config.yaml`、`IAM/config/iam_config.yaml` 仅提供本地占位值，必须替换所有 `CHANGE_ME` 并准备日志目录；它们不读取 shell 环境变量。正式部署请复制 `deploy/` 模板并按部署文档设置 MySQL、Redis、日志及端口。启动 DBOP 时可传配置文件路径；IAM 从工作目录读取配置：

```sh
./build/dbop DBOP/config/db_config.yaml
# 另一个终端，在 IAM 目录运行
cd IAM
../build/iam
```

在数据库 `service` 表中配置 `CROSSDO_WEB` 服务及其独立随机密码；将相同值通过 `CROSSDO_SERVICE_PASSWORD` 注入 Java。首名管理员由运维使用 `tools/bootstrap_admin.py` 对已注册并验证手机号的账号离线授权；随后在管理员后台核验并认证志愿者。没有开放注册管理员或自认证接口，操作方法见运营文档。

Java 支持下列环境变量：

| 变量 | 默认值 / 用途 |
| --- | --- |
| `SERVER_PORT` | `8080` |
| `CROSSDO_DBOP_URL` | `http://127.0.0.1:1900` |
| `CROSSDO_IAM_URL` | `http://127.0.0.1:10089` |
| `CROSSDO_SERVICE_NAME` | `CROSSDO_WEB` |
| `CROSSDO_SERVICE_PASSWORD` | 必填，和 `service` 表一致 |
| `SPRING_REDIS_HOST` / `SPRING_REDIS_PORT` | `127.0.0.1` / `6379` |
| `SPRING_REDIS_PASSWORD` | Redis 配置的密码 |
| `CROSSDO_SESSION_TTL` | `PT2H`，固定有效期 |
| `CROSSDO_SMS_MODE` / `CROSSDO_PAYMENT_MODE` | `disabled`；本地测试显式设为 `development`，接入服务用 `gateway` |
| `CROSSDO_UPLOAD_DIR` | `./var/uploads`；部署时指定持久化目录 |

```sh
java -jar java/org.crossdo/target/crossdo-0.0.1-SNAPSHOT.jar
```

C 服务可通过 `CROSSDO_BIND_ADDRESS=127.0.0.1` 仅监听本机（默认沿用 `0.0.0.0`）。只有 Java `/api` 面向终端用户；IAM、DBOP 和数据库应在内部网络使用。旧 `/user/*`、`/ledger/*` 和项目写接口已返回 410；业务统一走鉴权的 `/workflow/*`，避免绕过会话失效、权限和资金流水。

## 业务 API

请求和响应字段使用 `snake_case`。除认证设置、验证码、注册/找回密码以及符合公开条件的资金、结项和文件读取外，业务接口需要 `Authorization: Bearer <token>`；身份来自登录会话，提交 `actor_id`、权限、审核状态等字段不能改变操作者身份。成功返回数据对象或数组，错误为 `{"status":400,"message":"..."}`。项目金额使用十进制字符串，例如 `"10000.25"`。

| 方法与路径 | 行为 |
| --- | --- |
| `POST /api/auth/register` | `phone_number`、`user_name`、`password`、`user_permission`（2 捐助者 / 3 患者 / 5 志愿者），以及注册验证码 `challenge_id`、`code` |
| `POST /api/auth/verification-codes` | `phone_number`、`purpose`（register / password_reset / phone_change）；换号需登录 |
| `POST /api/auth/password-reset` | 手机号、验证码及新密码；撤销旧会话 |
| `POST /api/me/phone` | 当前密码、新手机号及验证码；撤销旧会话 |
| `POST /api/auth/login` | `phone_number`、`password`；返回 `token`、`token_type`、`expires_in`、`user` |
| `POST /api/auth/logout` | 撤销当前会话 |
| `GET /api/me` | 自己的资料，不含密码、身份证、OpenID |
| `POST /api/me/application` | `real_name`、`id_card`、`user_age`、`materials` 文件引用数组 |
| `GET /api/applications` | 志愿者注册审核任务池 |
| `GET /api/applications/{user_id}` | 患者自己或当前接单人读取资料 |
| `POST /api/applications/{user_id}/claim` | 领取患者资料审核 |
| `POST /api/applications/{user_id}/review` | `decision` 和 `note` |
| `POST /api/projects` | 已认证患者创建草稿；每人最多一个未结束项目 |
| `GET /api/projects` | 已通过项目列表 |
| `GET /api/projects/{project_id}` | 项目详情，依身份返回公开或审核视图 |
| `POST /api/projects/{project_id}/submit` | `donation_pool_limit`、`pathography` 数组 |
| `GET /api/tasks/projects` | 志愿者项目审核任务池 |
| `POST /api/projects/{project_id}/claim` | 领取项目审核 |
| `POST /api/projects/{project_id}/review` | `decision` 和 `note` |
| `POST /api/projects/{project_id}/pathography` | 追加 `entry`，包含 `content`、`attachments` 数组 |

列表接受 `limit`（1–100）和 `offset`（默认 0）。公开项目列表支持 `summary=true`：返回最多 240 字的 `summary`，不返回病情数组，适用于首页和项目卡片；省略时保持原完整列表响应，详情接口始终返回完整当前版本。前端当前页搜索匹配昵称和摘要。`decision` 为 `approve`、`correction`、`unreachable`；审核者必须为当前接单的已认证志愿者。病情条目格式为 `{"content":"病情说明","attachments":["https://文件地址"]}`；时间由服务器填写。申请材料仅对本人、当前审核者与管理员可见；项目当前已通过版本的附件可公开，旧版本和未绑定文件保持私有。举报证据仅举报人和管理员可读。

患者和项目都沿用设计中的状态：初稿 1 → 待领取 2 → 已领取 3 → 通过 0；补正 5 / 联系不上 4 → 重提交 6 → 已领取 7。通过前的项目不会出现在公开列表。项目结项依次进入 12（申请结项）和 13（结项完成）；批准前必须清结待处理支付/退款/用款及剩余余额。

新增文件、通知、运营、资金及结项 API 的配置和使用见 [运营与服务接入](设计/运营与服务接入.md)。

## 回归验证

`mvn test` 运行 Java 上下文、会话、API 校验和 IAM/DBOP HTTP 协议测试。C 编译检查使用 `make c`。SQL 的执行计划、十万级合成数据实测和复现方法见 [查询性能说明](设计/SQL查询性能说明.md)。

集成测试可使用独立本地栈（需要上述数据库与 C 依赖）：

```sh
tools/tests/start_workflow_stack.sh start
source /tmp/crossdo-review-stack/stack.env
# 在另一个终端使用同一份 stack.env 启动打包后的 Java 服务
python3 tools/tests/workflow_integration.py
python3 tools/tests/release_integration.py
python3 tools/tests/security_release_integration.py
python3 tools/tests/finance_release_integration.py
python3 tools/tests/closure_concurrency.py
tools/tests/start_workflow_stack.sh stop
```

测试栈使用独立临时数据目录及端口，不使用仓库示例中的远程服务。集成脚本会注册全新账号，并只在指定的测试数据库中认证本次创建的志愿者；不要将其指向生产环境。覆盖注册审核补正重提、项目审核、并发接单、越权、重复建项目、金额精度、病情追加和退出登录。测试配置文件含临时随机凭据，仅保存在本机临时目录。新增数据库回归可运行 `python3 tools/tests/schema_integration.py`（使用同一份 `stack.env`），在自动创建的临时数据库中检查外键、状态/金额约束、旧数据迁移、重复迁移和脏数据中止；只删除测试自身创建的临时库。工作流回归还覆盖超过 64 KiB 的审核及病情记录、提交快照和并发创建/追加。


## 前端开发与发布

前端位于 `frontend/`，使用 React、TypeScript 和 Vite；中文界面支持桌面与手机。页面连接真实 `/api`，患者可提交资料、查看审核意见并创建项目，认证志愿者可领取并处理资料及项目审核，捐助者可浏览已通过审核的项目。页面支持直接上传材料、验证码注册/找回/换号、捐赠退款、用款及结项；管理员可处理认证、任务、案件和资金审批。

开发环境需要 Node.js 20.19+ 或 22.12+，先启动现有后端。默认 Vite 将 `/api` 代理至本机 `18080`（与隔离测试栈一致）；如果 Java 使用默认 `8080`，设置 `CROSSDO_API_PROXY=http://127.0.0.1:8080`。

```sh
cd frontend
npm ci
npm run dev
```

打开 `http://localhost:5173/`。页面使用 HashRouter，刷新详情页不会丢失路由。登录凭据仅在当前标签页会话内保存，个人资料留在内存，密码、身份证及医疗表单不写入浏览器持久存储。退出、过期和 API 401 会清除当前会话；暂时的网络错误保留会话并提供恢复入口。

构建完整发布包：

```sh
make package
# 等价于先 frontend/npm ci + npm run build，再 java/org.crossdo/mvn test package
```

Maven 会将 `frontend/dist` 打包到 JAR 的静态资源中。随后启动 `java/org.crossdo/target/crossdo-0.0.1-SNAPSHOT.jar`，通过 Java 服务地址直接打开前端，无需单独的 Node 服务。只运行 `mvn package` 时，需要先构建前端资源。C 服务仍按前述方式独立运行。

浏览器回归使用 Playwright，覆盖真实注册登录、患者材料补正与重提、志愿者接单审核、项目申请与公开病情进展，以及手机导航、服务异常与会话过期。

```sh
# 从仓库根目录执行；在单独终端分别启动 Java 和前端 dev 服务
source /tmp/crossdo-review-stack/stack.env
cd frontend
npm run test:e2e
```

测试默认调用 `/usr/bin/chromium`，可通过 `CHROMIUM_PATH` 指定系统浏览器。测试账号随机创建，仅在独立测试数据库中认证本次创建的志愿者；不要使用生产数据库。截图和 trace 输出到 `frontend/test-results/`，不提交到版本库。
