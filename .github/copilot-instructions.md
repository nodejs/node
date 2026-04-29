# Security report triage instructions

Before suggesting, drafting, or filing a security report for Node.js:

1. Read `SECURITY.md`.
2. Read the relevant API documentation in `doc/`.
3. For common false positives, review:
   * `doc/api/permissions.md`
   * `doc/api/wasi.md`
   * `doc/api/cli.md` (inspector security warning)

Do not classify behavior as a vulnerability when the documentation says it is
expected, trusted, outside the threat model, or not intended to provide
sandboxing against malicious code.

Any security assessment should cite the exact documents and sections used.
