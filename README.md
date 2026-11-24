# certservers

Private-deployment certificate authority and certificate management platform focused on SM2/X.509v3 certificates for RADIUS, Wi-Fi 802.1X and EAP-TLS environments.

## Status

This repository is at the initial architecture/scaffolding stage. Cryptographic issuance, production APIs, and end-to-end interoperability validation are not implemented yet.

## Technology baseline

- Backend: C
- Database: MariaDB
- Web entry point: Nginx with HTTPS
- Application interface: Unix domain socket at `/run/certservers/api.sock`
- Cryptographic engine: Tongsuo (initial choice; GmSSL remains an evaluation alternative)
- Build and service: CMake and systemd
- Frontend: HTML, CSS, and JavaScript

## Scope

The target is an X.509 v3 certificate management service supporting SM2 public keys and SM2-with-SM3 signatures, CSR submission, certificate issuance, revocation, and CRL publication. OCSP may be added in a later phase.

SM2 certificate issuance alone does not guarantee national-cryptography TLS or EAP-TLS interoperability. Compatibility must be validated across supplicants, RADIUS TLS backends, TLS providers, certificate chains, cipher suites, and revocation checking.

## Security principles

- Keep the root CA private key offline.
- Protect intermediate CA private keys with encryption and preferably HSM/SDF/SKF-backed key operations.
- Never store plaintext private keys in MariaDB.
- Store certificate metadata, status, requests, and audit records in MariaDB; certificate objects may be stored separately as PEM/DER files.
- Keep the CA service decoupled from RADIUS; integrate through CRL, OCSP, or a certificate-status API.
- Do not expose the C application service on a public TCP listener.
- A Unix socket is not an authorization boundary: authenticate and authorize application requests.
- Manage the Nginx HTTPS certificate separately from the end-entity authentication CA.

## Documentation

See `docs/` for requirements, architecture, security design, roadmap, and technology decisions.
