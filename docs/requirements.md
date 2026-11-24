# Requirements

## Purpose
Provide a privately deployed certificate authority and certificate management service for RADIUS, Wi-Fi 802.1X, and EAP-TLS terminal authentication.

## Functional requirements
- Support X.509 v3 certificates with SM2 public keys and SM2-with-SM3 signatures.
- Accept and validate PKCS#10 CSRs.
- Issue, inspect, revoke, and export certificates.
- Publish CRLs; consider OCSP in a later phase.
- Maintain CA, certificate, request, revocation, and audit metadata.
- Provide an administrative web interface and versioned API.

## Non-functional requirements
- C backend, MariaDB persistence, Nginx HTTPS entry point.
- Nginx proxies API traffic to a Unix domain socket.
- C service does not expose a public TCP listener.
- Build with CMake and run under systemd.
- Apply authentication, authorization, audit, input validation, and secure key handling.

## Interoperability requirement
SM2 X.509 issuance does not by itself establish national-cryptography TLS or EAP-TLS compatibility. Validate the complete path: endpoint supplicant, RADIUS TLS backend, TLS provider, trust chain, algorithms/cipher suites, and revocation behavior.
