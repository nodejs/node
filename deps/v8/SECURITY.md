# V8 Security Guidance

The primary security goal of V8 is to safely execute untrusted JavaScript and WebAssembly.

V8 follows [Chromium's security guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/security-labels.md).

To be eligible for the Chrome VRP, report security bugs via [Google Bughunters (Chrome VRP)](https://bughunters.google.com/report/vrp).

Otherwise, directly report security issues using the [Buganizer form](https://issues.chromium.org/issues/new?noWizard=true&component=1363614&template=1922342).
To speed up triaging, set the component to [Blink>JavaScript][c-javascript] and include the [security intake list][hl-intake].

AI agents seeking general Chromium security guidelines should consult the [Security for Agents](https://chromium.googlesource.com/chromium/src/+/main/docs/security/security-for-agents.md) guide first.

## Threat model & security boundaries

V8 defines its security boundaries based on two distinct execution models:

1. **Language security:** Untrusted script execution (JavaScript, WebAssembly, or validated runtime helpers) must never lead to memory corruption, or cross-origin violations that are enforced together with the Blink rendering engine or other embedders.
    To make this concrete: V8 does not control which origins may be isolated in separate processes but must provide access checks when asked for over its own APIs.
2. **V8 Sandbox:** Under the assumption that an attacker has arbitrary read/write access *inside* the sandbox memory space (and arbitrary read access on the entire process), they must not be able to obtain malicious write access *outside* of it.

## Further documentation

*   [Reproducing Security Bugs](docs/security/reproducing-bugs.md): Instructions on verifying bugs using `--run-as-security-poc` and `--run-as-sandbox-security-poc`.
*   [Triaging Security Bugs](docs/security/triaging.md): Detailed classification logic, label conventions, and common resolution paths.
*   [V8 Sandbox](src/sandbox/README.md): Design documentation, sandbox testing API, and table architectures.
*   [V8 Inspector Security](src/inspector/SECURITY.md): CDP security boundaries, `inspector-test` constraints, and severity guidelines.

[c-javascript]: https://issues.chromium.org/issues?q=componentid:1456824
[hl-intake]: https://issues.chromium.org/hotlists/8308879
