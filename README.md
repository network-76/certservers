# certservers

面向私有化部署的证书颁发与管理平台，重点支持 RADIUS、Wi-Fi 802.1X 和 EAP-TLS 终端身份认证场景下的 SM2 / X.509 v3 证书管理。

## 项目状态

当前仓库处于初始架构与工程脚手架阶段。证书密码学签发、正式业务 API，以及端到端互通性验证尚未实现。

## 技术选型

- **后端：** C 语言
- **数据库：** MariaDB
- **Web 接入：** Nginx，提供 HTTPS
- **应用接口：** Unix Domain Socket，默认路径为 `/run/certservers/api.sock`
- **密码引擎：** 初步选择铜锁（Tongsuo）；GmSSL 作为备选评估方案
- **构建与服务管理：** CMake、systemd
- **前端：** HTML、CSS、JavaScript

## 项目范围

目标是建设一套 X.509 v3 证书管理服务，支持：

- SM2 公钥及 SM2-with-SM3 签名算法
- PKCS#10 CSR 申请与校验
- 证书签发、查询、吊销与导出
- CRL 生成和发布
- OCSP 可在后续阶段评估加入

**重要说明：** 能够签发 SM2 X.509 证书，并不代表已经实现国密 TLS 或 EAP-TLS 互通。必须结合实际终端 supplicant、RADIUS TLS 后端、TLS 密码库、证书链、密码套件及吊销检查机制进行端到端验证。

## 安全原则

- 根 CA 私钥应离线保存。
- 中间 CA 私钥应加密保护；条件允许时优先使用 HSM / SDF / SKF 等硬件密码设备进行密钥操作。
- 严禁在 MariaDB 中存储明文私钥。
- MariaDB 用于保存证书元数据、状态、申请记录和审计信息；证书文件可独立以 PEM / DER 格式存储。
- CA 系统与 RADIUS 解耦，通过 CRL、OCSP 或证书状态 API 协作。
- C 业务服务不直接开放公网 TCP 监听。
- Unix Socket 权限控制不等于业务授权；应用仍必须执行身份认证与权限校验。
- Nginx 的 Web HTTPS 证书应与终端身份认证 CA 分开管理。

## 文档导航

- [需求说明](docs/requirements.md)
- [系统架构](docs/architecture.md)
- [安全设计](docs/security.md)
- [开发路线图](docs/roadmap.md)
- [技术选型决策记录](docs/decisions/0001-technology-baseline.md)
