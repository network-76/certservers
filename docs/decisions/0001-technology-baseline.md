# ADR 0001: Technology Baseline

- **Status:** Accepted as initial implementation baseline; crypto engine remains subject to validation.
- **Backend:** C, modular single-process service initially.
- **Database:** MariaDB.
- **Web:** Nginx terminates HTTPS and proxies API calls over Unix domain socket.
- **Socket:** `/run/certservers/api.sock`, dedicated service user/group, typically mode 0660.
- **Crypto:** Tongsuo as the first implementation candidate; evaluate GmSSL separately, but do not build dual-engine support prematurely.
- **Build/runtime:** CMake and systemd.
- **Frontend:** HTML, CSS, JavaScript.

## Rationale
A small deployment footprint and explicit trust boundaries suit private deployments. A modular C service keeps the initial operational model simple while allowing later process separation.

## Consequences
SM2 X.509 support and EAP-TLS interoperability are separate acceptance criteria. The latter requires end-to-end testing against the intended endpoint and RADIUS stacks.
