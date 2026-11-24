# Security Design

- Keep the root CA private key offline.
- Protect intermediate CA keys with encryption at rest and preferably HSM/SDF/SKF-backed operations.
- Never store plaintext private keys in MariaDB.
- Restrict filesystem permissions for keys, certificate storage, configuration, and Unix socket.
- Use a dedicated service account and group. Socket mode should normally be 0660; its parent directory must not be world-writable.
- Do not trust a request merely because it arrived over a Unix socket. Enforce application authentication and authorization.
- Apply role-based access control, CSRF protection where applicable, rate limits, strict input validation, and security audit logging.
- Protect administrative sessions and secrets; avoid logging private key material, passwords, tokens, or full sensitive payloads.
- Back up database metadata and certificate objects securely; document key backup and recovery separately.
- Separate the web HTTPS certificate lifecycle from the client-authentication CA lifecycle.
- Define and test revocation, CRL publication, clock synchronization, and CA compromise procedures.
