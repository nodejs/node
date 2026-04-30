# AI agent instructions for security reports

Before suggesting, drafting, or filing a security report for this repository:

1. Read [`SECURITY.md`](./SECURITY.md) first, especially:
   * "The Node.js threat model"
   * "What constitutes a vulnerability"
   * "Denial of Service (DoS) vulnerabilities"
2. Read the API documentation for the feature involved in the report.
3. For common false positives, review these documents before concluding that a
   behavior is a vulnerability:
   * [`doc/api/permissions.md`](./doc/api/permissions.md)
   * [`doc/api/wasi.md`](./doc/api/wasi.md)
   * [`doc/api/cli.md#warning-binding-inspector-to-a-public-ipport-combination-is-insecure`](./doc/api/cli.md#warning-binding-inspector-to-a-public-ipport-combination-is-insecure)

Do not recommend a security report when the behavior is already documented as:

* expected behavior;
* outside the Node.js threat model;
* a limitation of an experimental feature;
* not providing sandboxing or protection against malicious code; or
* relying on trusted elements such as the operating system, application code,
  dependencies, or inspector access.

When producing a security assessment for Node.js:

* cite the exact document and section used;
* explain whether the behavior conflicts with a documented Node.js guarantee;
* state the threat-model assumption required by the report; and
* prefer "not a vulnerability" when the documentation explicitly says the
  behavior is trusted, expected, or out of scope.

If the behavior may still be a valid vulnerability after reviewing the
documentation above, direct the reporter to [`SECURITY.md`](./SECURITY.md) for
the private reporting process via HackerOne.
