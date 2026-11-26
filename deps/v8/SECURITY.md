# V8 Security Guidance

The primary security goal of V8 is to safely execute untrusted JavaScript and WebAssembly.

V8 follows Chromium's security policies ([Security FAQ](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/faq.md), [Security Rules](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rules.md), [Severity Guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/severity-guidelines.md), and [Security Labels](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/security-labels.md)).
AI agents seeking general Chromium security guidelines should consult the [Security for Agents](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/security-for-agents.md) guide first.

## Reporting security bugs

To be eligible for the Chrome VRP, report security bugs via [Google Bughunters (Chrome VRP)](https://bughunters.google.com/report/vrp).

Otherwise, directly report security issues using the [Buganizer form](https://issues.chromium.org/issues/new?noWizard=true&component=1363614&template=1922342).
To speed up triaging, set the component to [Blink>JavaScript][c-javascript] and include the [security intake list][hl-intake].

## Scope

This policy covers production V8 engine code shipped to embedders (`src/` and `include/`, along with vendored runtime dependencies).
Subcomponents may provide their own security guidance, which takes precedence for their directories:

*   `src/inspector/`: [V8 Inspector Security](src/inspector/SECURITY.md)

Non-production code is out of scope.
This includes tests, samples, experiments, and build, developer, or fuzzing tools (e.g., `test/`, `testing/`, `tools/`, `samples/`, `experimental/`, and the `d8` shell in `src/d8/`).

Architecture ports and projects not officially supported by Chromium are still tracked as security issues, but with no Chrome security impact.
See [Triaging Security Bugs](docs/security/triaging.md#officially-unsupported-ports-or-projects) for the current list.

## Threat model & security boundaries

V8 defines its security boundaries based on two distinct execution models:

1. **Language security:** Untrusted script execution (JavaScript, WebAssembly, or validated runtime helpers) must never lead to memory corruption, or cross-origin violations that are enforced together with the Blink rendering engine or other embedders.
    To make this concrete: V8 does not control which origins may be isolated in separate processes but must provide access checks when asked for over its own APIs.
2. **V8 Sandbox:** Under the assumption that an attacker has arbitrary read/write access *inside* the sandbox memory space (and arbitrary read access on the entire process), they must not be able to obtain malicious write access *outside* of it.

### Process type & sandbox

V8 processes untrustworthy input (JavaScript and WebAssembly) using memory-unsafe code (C++ and JIT-generated machine code).
Across all embedding contexts, embedders executing untrusted code must always isolate V8 inside an OS-sandboxed process.

In Chromium, V8 complies with the [Rule of Two](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md) by running inside an OS-sandboxed child process rather than an unsandboxed process like the Browser process.
This includes strongly sandboxed Renderer processes and sandboxed Utility or isolated service processes, with untrusted scripts running at Chromium's lowest privilege tier.

An OS process sandbox isolates the host process itself.
On 64-bit platforms, the V8 Sandbox adds an in-process boundary that protects the rest of the host process from memory corruption in V8.

### Inputs & trust assumptions

*   **Untrusted inputs:** JavaScript, WebAssembly, validated runtime helpers (for fuzzing), and, under the V8 Sandbox model, all memory inside the sandbox address space.
*   **Trusted inputs:** Embedder code and C++ API preconditions, startup snapshots, command-line flags, and, under the V8 Sandbox model, out-of-sandbox trusted objects and metadata.

V8 exposes no IPC endpoints of its own (such as Chromium Mojo interfaces); cross-process IPC and origin validation are handled by the embedder.

V8 does not protect against an already-compromised embedder process (native code execution outside the V8 Sandbox).
In Chromium, this means V8 does not defend against a compromised renderer process, as a V8 isolate is not a process-level boundary.
Cross-site protection relies on Chromium's [Site Isolation](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/process_model_and_site_isolation.md) and [compromised renderer defenses](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/compromised-renderers.md), or equivalent OS process isolation in other embedders.

## Further documentation

*   [Reproducing Security Bugs](docs/security/reproducing-bugs.md): Instructions on verifying bugs using `--run-as-security-poc` and `--run-as-sandbox-security-poc`.
*   [Triaging Security Bugs](docs/security/triaging.md): Detailed classification logic, label conventions, and common resolution paths.
*   [V8 Sandbox](src/sandbox/README.md): Design documentation, sandbox testing API, and table architectures.

[c-javascript]: https://issues.chromium.org/issues?q=componentid:1456824
[hl-intake]: https://issues.chromium.org/hotlists/8308879
