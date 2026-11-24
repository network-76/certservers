# Database migrations

Store ordered, forward-only SQL migrations here, for example `0001_initial_schema.sql`.

Guidelines:
- Use InnoDB and utf8mb4 where appropriate.
- Store private key references or encrypted key blobs only; never plaintext private keys.
- Keep certificate DER/PEM payload storage policy explicit.
- Include migration version tracking and test upgrade paths on a disposable MariaDB instance.
- Record issuance, revocation, and privileged administrative actions in an append-only audit design where practical.
