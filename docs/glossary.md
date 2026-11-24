# 专业术语表

本文档统一解释项目中常见的密码学、数字证书、网络认证和软件工程术语。其他文档中术语第一次出现时，应尽量采用“中文名称（英文全称，缩写）”的写法，并给出简短说明。

| 术语 / 缩写 | 中文名称 | 英文全称 | 说明 |
|---|---|---|---|
| CA | 证书颁发机构 | Certificate Authority | 负责签发、管理和吊销数字证书的可信机构或系统。 |
| Root CA | 根证书颁发机构 | Root Certificate Authority | 证书信任链的起点，其 CA 证书通常由自身签名；私钥应离线保护。 |
| Intermediate CA | 中间证书颁发机构 | Intermediate Certificate Authority | 由上级 CA 签发证书的下级 CA，用于分担签发职责并隔离根密钥风险。 |
| X.509 | 公钥证书标准 | X.509 Public Key Certificate | 定义公钥证书的数据结构、字段和扩展规则；v3 版本支持扩展字段。 |
| CSR | 证书签名请求 | Certificate Signing Request | 申请者提交给 CA 的请求，通常包含公钥、主体信息和申请者签名；CA 应验证签名及申请策略。 |
| PKCS#10 | 证书请求语法标准 | Certification Request Syntax Standard | 定义常见 CSR 的数据格式。 |
| CRL | 证书吊销列表 | Certificate Revocation List | CA 签名发布的吊销证书清单，验证方可据此检查证书序列号是否已吊销。 |
| OCSP | 在线证书状态协议 | Online Certificate Status Protocol | 用于在线查询单张证书的状态，例如未发现吊销记录、已吊销或未知。 |
| CDP | CRL 分发点 | CRL Distribution Points | X.509 证书扩展字段，指示验证方从哪里获取 CRL。 |
| SAN | 主题备用名称 | Subject Alternative Name | X.509 证书扩展字段，用于标识 DNS 名称、IP 地址、电子邮件等身份信息。 |
| EKU | 扩展密钥用途 | Extended Key Usage | 限制证书可用于哪些用途，例如客户端认证或服务器认证。 |
| PKI | 公钥基础设施 | Public Key Infrastructure | 管理密钥、证书、信任关系、签发和吊销的一整套体系。 |
| SM2 | SM2 椭圆曲线公钥密码算法 | SM2 Public Key Cryptographic Algorithm | 中国国家密码标准中的公钥密码算法。 |
| SM3 | SM3 密码杂凑算法 | SM3 Cryptographic Hash Algorithm | 中国国家密码标准中的密码杂凑算法。 |
| PEM | 隐私增强邮件格式 | Privacy-Enhanced Mail | 常见的文本封装格式，通常以 Base64 表示证书或密钥并带有头尾标记。 |
| DER | 可分辨编码规则 | Distinguished Encoding Rules | ASN.1 数据的一种二进制编码格式，常用于证书文件。 |
| HSM | 硬件安全模块 | Hardware Security Module | 用于保护密钥并在受控硬件环境中执行密码运算的设备。 |
| RADIUS | 远程认证拨入用户服务 | Remote Authentication Dial-In User Service | 网络接入领域常用的认证、授权和计费协议。 |
| 802.1X | 基于端口的网络访问控制 | Port-Based Network Access Control | 在有线或无线网络接入时执行身份认证和端口授权的机制。 |
| EAP | 可扩展认证协议 | Extensible Authentication Protocol | 为多种认证方法提供统一承载框架的协议。 |
| EAP-TLS | 基于 TLS 的可扩展认证协议 | EAP-TLS | 使用 TLS 和数字证书完成认证的 EAP 方法，通常可实现客户端与服务端双向认证。 |
| Supplicant | 认证请求方 | Supplicant | 802.1X 体系中发起认证的一方，通常是终端设备上的认证客户端。 |
| TLS | 传输层安全协议 | Transport Layer Security | 为网络通信提供加密、完整性保护和身份认证的安全协议。 |
| mTLS | 双向 TLS 认证 | Mutual TLS | TLS 通信双方都出示并验证证书，而不只是客户端验证服务器。 |
| API | 应用程序编程接口 | Application Programming Interface | 软件组件之间约定的调用接口；本项目主要指 HTTP 管理接口。 |
| Unix Domain Socket | Unix 域套接字 | Unix Domain Socket | 同一操作系统内进程间通信的一种方式，可通过文件系统路径建立本地连接。 |
| RBAC | 基于角色的访问控制 | Role-Based Access Control | 将权限授予角色，再将角色分配给用户的授权模型。 |
| CSRF | 跨站请求伪造 | Cross-Site Request Forgery | 攻击者诱导已登录用户浏览器向目标网站发起非预期请求的攻击方式。 |
| Fail-closed | 故障关闭 | Fail-closed | 关键安全检查无法完成时拒绝操作，避免因故障意外放行。 |
| Fail-open | 故障开放 | Fail-open | 关键检查故障时仍允许操作，提升可用性但可能增加安全风险。 |
| CMake | 构建配置工具 | CMake | 用于生成项目构建系统的工具，本项目用于管理 C 代码构建。 |
| systemd | Linux 系统与服务管理器 | systemd | 用于启动、停止、监控和管理 Linux 系统服务。 |

## 术语使用约定

1. 第一次出现时给出中文名称和英文缩写；英文全称在有助于理解时一并给出。
2. 后续可直接使用缩写，但不得改变术语含义。
3. 区分“协议具备某项能力”和“目标软件已实现该能力”。
4. 产品名称、标准编号、算法标识、代码符号和配置项保留原始拼写。
