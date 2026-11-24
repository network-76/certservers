# Roadmap

## Phase 0 — Foundation
- [x] Establish technology baseline and repository structure.
- [ ] Define API conventions, database migration policy, and threat model.
- [ ] Establish CI build and static-analysis checks.

## Phase 1 — CA foundation
- [ ] Integrate selected crypto engine (Tongsuo evaluation).
- [ ] Implement CA key and certificate lifecycle.
- [ ] Implement CSR parsing and strict profile validation.
- [ ] Implement SM2 certificate issuance and verification tests.

## Phase 2 — Certificate lifecycle
- [ ] Certificate inventory and search.
- [ ] Revocation and CRL generation/publication.
- [ ] Audit trail and administrative RBAC.

## Phase 3 — Web/API
- [ ] Authentication and session management.
- [ ] Versioned API and administrative UI.
- [ ] Nginx and systemd deployment hardening.

## Phase 4 — Interoperability
- [ ] Test target supplicants and operating systems.
- [ ] Test RADIUS TLS backend and crypto provider combinations.
- [ ] Verify chain building, cipher negotiation, and revocation checking.
- [ ] Evaluate OCSP and HSM/SDF/SKF integration.
