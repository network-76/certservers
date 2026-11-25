# certservers

私有化部署的数字证书颁发与管理平台，面向 RADIUS、Wi-Fi 802.1X 和 EAP-TLS 终端身份认证场景，重点支持 SM2 国密算法及 X.509 v3 数字证书。

## 项目当前状态

当前仓库处于初始架构与工程脚手架阶段。证书密码学签发、完整业务接口、证书生命周期管理以及端到端互通性验证尚未实现。仓库中的配置和部署文件是开发起点，正式部署前必须经过安全审查和环境验证。

## 技术选型

- **后端：** C 语言，初期采用单进程、模块化设计。
- **数据库：** MariaDB，用于保存证书元数据、申请记录、状态和审计信息。
- **Web 接入：** Nginx，负责 HTTPS 接入、静态页面服务和 API 反向代理。
- **应用接口：** Unix Domain Socket（Unix 域套接字），默认路径为 `/run/certservers/api.sock`。
- **密码引擎：** 初步选择铜锁（Tongsuo）；GmSSL 作为备选评估方案，初期不同时维护两套密码引擎。
- **构建与服务管理：** CMake、systemd。
- **前端：** HTML、CSS、JavaScript。

## 项目目标与功能范围

目标是建设一套符合 X.509 v3 标准的证书管理服务，初步支持：

- SM2 公钥算法及 SM2-with-SM3 数字签名算法。
- CSR（证书签名请求）：接收并验证申请者提交的公钥和证书主体信息。
- 数字证书签发、查询、吊销和导出。
- **PEM 与 P12 两种交付格式：** PEM 用于证书、证书链及密钥材料的文本封装；P12（PKCS#12，也常见扩展名 .p12/.pfx）用于将证书、私钥及证书链封装为带密码保护的文件。两种格式承载的是同一张 X.509 证书，不代表不同证书类型。
- CRL（证书吊销列表）：由 CA 发布已吊销证书清单，供验证方下载检查。
- OCSP（在线证书状态协议）：允许验证方在线查询单张证书的状态，作为后续阶段评估功能。
- CA 层级、证书模板、操作审计和管理员权限管理。

关于 PEM/P12 的导出内容、私钥归属和安全要求，详见[《证书格式与密钥交付规范》](docs/certificate-formats.md)。

## 重要的互通性说明

能够签发 SM2 X.509 证书，并不代表已经实现国密 TLS 或 EAP-TLS 互通。必须结合实际终端认证客户端（supplicant）、RADIUS 的 TLS 后端、TLS 密码库、信任链、密码套件以及吊销检查机制进行端到端验证。证书格式、签名算法和 TLS 协议协商是不同层次的问题。

## RADIUS 与 CA 的协作方式

CA 与 RADIUS 保持解耦，不直接共享数据库，也不建议让 RADIUS 直接查询 CA 内部表。

- **首选方式：CRL 分发。** CA 生成并发布 CRL，RADIUS 服务器按配置地址定期通过 HTTPS 下载，校验签名和有效期后更新本地缓存，并在客户端证书验证时检查吊销状态。
- **增强方式：OCSP。** 当需要更及时的单证书状态查询时，再部署 OCSP 响应服务，并确认 RADIUS TLS 后端确实支持相应的 OCSP 检查。
- **补充方式：证书状态 API。** 仅在 CRL/OCSP 无法满足业务需求时考虑。应使用 HTTPS、双向 TLS 或强身份凭据、最小权限和审计；不可将其作为绕过标准证书验证的捷径。

详细设计见[《RADIUS 与 CA 集成方案》](docs/radius-integration.md)。

## 安全原则

- 根 CA（证书颁发机构）私钥应离线保存。
- 中间 CA 私钥应加密保护；条件允许时优先使用 HSM（硬件安全模块）、SDF 或 SKF 等密码设备。
- 严禁在 MariaDB 中存储明文私钥。
- MariaDB 保存业务元数据、证书状态、申请记录和审计信息；证书文件可独立以 PEM 或 DER 格式存储。
- 客户端 CSR 模式下，终端私钥原则上留在终端，不上传 CA 平台；只有在明确选择平台生成密钥并执行受控导出时，才允许将终端私钥放入 P12 文件。
- C 业务服务不直接开放公网 TCP 监听，由 Nginx 通过 Unix 域套接字转发请求。
- Unix 套接字的文件权限不能替代应用层身份认证和权限校验。
- Nginx 管理页面使用的 HTTPS 证书，应与终端身份认证 CA 分开管理。

## 文档导航

- [专业术语表](docs/glossary.md)
- [需求说明](docs/requirements.md)
- [系统架构](docs/architecture.md)
- [证书格式与密钥交付规范](docs/certificate-formats.md)
- [RADIUS 与 CA 集成方案](docs/radius-integration.md)
- [安全设计](docs/security.md)
- [开发路线图](docs/roadmap.md)
- [技术选型决策记录](docs/decisions/0001-technology-baseline.md)

## 文档语言约定

项目说明文档统一使用中文。专业术语第一次出现时，尽量给出中文名称、英文全称、缩写和简要解释；代码符号、协议名称、标准编号、文件名及 API 路径保留其标准写法。


## 领域模板与 LDAP 身份验证

平台支持按业务领域配置证书申请模板。申请人选择领域后，系统根据模板自动填充证书主体、主题备用名称（SAN）、密钥用途、扩展密钥用途、有效期和其他申请字段；字段可配置为系统生成、LDAP 自动获取、用户填写、用户选择或管理员锁定。管理员可自定义模板和字段规则，后端始终按模板版本执行校验。

证书申请可对接标准 LDAP（轻量级目录访问协议）服务器。申请人先通过 LDAP 身份验证，平台再读取授权范围内的目录属性，并根据领域模板、用户组、申请内容和审批策略，决定自动签发、转人工审核或拒绝。LDAP 验证成功不代表证书自动获批。

详细设计：
- [证书领域模板与字段规则](docs/domain-templates.md)
- [LDAP 身份验证与证书申请审核](docs/ldap-integration.md)


## LDAP、设备身份与 WAPI 专项设计

- [LDAP 对接与证书申请审批需求](docs/ldap-application-requirements.md)
- [LDAP 身份验证与证书申请审核设计](docs/ldap-integration.md)
- [设备身份与设备组管理设计](docs/device-identity.md)
- [WAPI 证书格式与签发要求](docs/wapicert-profile.md)
- [WAPI 证书格式规范原文及配套图片](docs/wapicert/wapicert.md)

- [项目技术尽调与风险评估报告（初步）](docs/technical-audit.md)
