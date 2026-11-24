# Architecture

## Components
- **Nginx**: terminates HTTPS, serves the administrative frontend, and proxies `/api/v1/` to the application socket.
- **C application**: initially one modular process; owns API routing, authorization, CA/certificate workflows, database access, cryptographic operations, and audit.
- **MariaDB**: stores metadata, certificate lifecycle state, requests, and audit records.
- **Certificate storage**: PEM/DER certificate objects stored separately from relational metadata.
- **Crypto engine**: Tongsuo is the initial candidate. Keep crypto operations behind an internal adapter to avoid spreading provider-specific calls.

## Request path
Browser → HTTPS/Nginx → `/run/certservers/api.sock` → C application → MariaDB / certificate storage / crypto engine.

## Process model
Start with one C service process and clear modules. Split background jobs or workers only when operational load requires it.

## RADIUS integration
Keep RADIUS independent of CA internals. Publish CRLs and, if later implemented, OCSP; alternatively expose a narrowly scoped certificate-status API.

## Trust domains
The Nginx web-server TLS certificate is separate from the end-entity authentication CA hierarchy.
