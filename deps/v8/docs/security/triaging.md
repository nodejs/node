# Security Bug Triaging

V8 generally triages security bugs based on [Chromium's guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/security-labels.md).

Google-only: The internal version of this document is available at [go/v8-security-gardening](http://goto.google.com/v8-security-gardening).

## Labels and classifications

These classifications refer to Buganizer which is the issue tracker used by Chromium.

- **Type=Vulnerability**: Designates a security vulnerability that impacts users.
- **Severity**: Same as [Chromium's severities](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/severity-guidelines.md).
- **Priority**: A priority that in general is at least the severity.
In certain circumstances, e.g., in-the-wild exploitation, we may raise the priority further.
- **Security_Impact-{Head, Beta, Stable, Extended, None}** hotlists: Derived from the milestones set in the **Found In** field, this hotlist specifies the earliest affected release channel.
Should not normally be set by humans, except in the case of **Security_Impact-None** (hotlistid: 5433277) which means that the bug is in a disabled feature, or otherwise doesn't impact Chrome: see the section below for more details.

In addition, the following fields are set as part of triaging:
- **Found In**: Should point to the milestone this was discovered to be broken.
It is okay to just set to the current stable or extended stable milestone if unknown.
- **Introduced In**: Should point to the milestone this was introduced.
It is okay to be conservative if unknown, e.g., to assume that the bug was present when a feature was launched.

### Sandbox bypasses

V8 Sandbox bypasses are included in Chrome’s VRP.
A successful bypass must show write access outside of the sandbox.
Read access is not considered part of the attack model.

These bugs are currently treated differently from regular security bugs.
Specifically, the following properties are different from regular security bugs:

- **Severity**: S2
- **Security_Impact-None** (hotlistid: 5433277)
- **v8-sandbox** (hotlistid: 4802478)

## Reproducing security bugs

Security bugs should have proof-of-concept reproductions (POCs) attached to them.
V8 currently still accepts bugs without a POC with the caveat that such bugs have a much higher chance of being dismissed quickly.
See [the detailed documentation](reproducing-bugs.md) for the various different configurations V8 can be put in.

### Regular security bugs

Bugs should reproduce on `d8` by providing `--run-as-security-poc` in addition to possibly other flags.
Additional stress flags may be passed and can be useful to trigger compiler tiering and garbage collection.
Bugs that only reproduce without `--run-as-security-poc` have a much higher chance of not being considered security bugs in the first place.
See the section below for common scenarios that lead to reclassifications.

### Sandbox security bugs

The same general principles apply to sandbox security bugs.
To ease classification, bugs should reproduce on `d8` by providing `--run-as-sandbox-security-poc` in addition to possibly other flags in the [sandbox testing environment](../../src/sandbox/README.md#testing).
A successful reproduction must result in the crash filter reporting a sandbox violation (e.g. "V8 sandbox violation detected").
Crashes reported as "harmless" by the filter do not qualify as sandbox escapes.

## Common cases for conditional features and code

This section lists common triaging scenarios.

### Bugs in code that is not experimental and is enabled by default

Fields: **Type=Vulnerability**, **Security_Impact-{Head,Beta,Stable,Extended}**

Rationale: Security bugs reachable through production code for regular users.

### Bugs in code that is not experimental but not enabled by default

Fields: **Type=Vulnerability**, **Security_Impact-None**

Rationale: These bugs are in features that are generally complete and on the track of shipping at some point.
We encourage experimenting and finding bugs in those features.

Note that **Severity** should still be set to the appropriate Severity (S0-S3) for **Security_Impact-None** issues, as if the feature were enabled or the code reachable.

### Bugs in code guarded by experimental flags

Fields: **Type=Bug**, **Security_Impact-None**

Rationale: The flags and setups guard unfinished features that are explicitly not considered ready for fuzzing yet.
Flags often follow the naming of `--experimental-*` and imply the `--experimental` flag.
Sometimes these flags also have some experimental annotation in the flag descriptions.

Note: If the flag is part of, e.g., `--future` or `--wasm-staging` then this signals that the flags are ready for fuzzing.
We don’t change the flag names in this case to avoid further churn in the codebase.

### Bugs in developer flags such as `--trace-*` or flags that are clearly marked as unsafe

Fields: **Type=Bug**, **Security_Impact-None**

Rationale: Not reachable in production, as these flags are only used by developers.

## Other common cases

### `nullptr` (or close to `0`) dereference

Fields: **Type=Bug**, **Security_Impact-None**

V8 relies on `nullptr` dereferences to deterministically crash.

### Bogus `DCHECK`s or reliable `CHECK` crashers

Fields: **Type=Bug**, **Security_Impact-None**

Rationale: These issues are not considered security vulnerabilities because the crashes either cannot happen in production builds (e.g., `DCHECK` failures, which are compiled out) or they result in a safe, deterministic crash (e.g., `CHECK` failures).
Invalid ("bogus") `DCHECK`s should still be fixed or removed.

Note: `CHECK`s must not be behind special builds or phases, such as `--verify-*`.

### Sandbox: Reliance on libc++ hardening

V8's sandbox security boundary relies on libc++ hardening (specifically
`_LIBCPP_HARDENING_MODE`) to prevent out-of-bounds accesses in standard
containers like `std::vector` and `std::span`.

Rationale: If a logic flaw allows an attacker to corrupt an index used for a
`std::vector::operator[]` access, the hardened STL will perform a bounds check
at runtime and safely trap (crash) even in release builds. Running into a
hardened libc++ check after an in-sandbox corruption is not a sandbox escape
and is not even considered a bug.

Note: Developers should still prefer using `SBXCHECK` or similar V8-specific
hardened checks for untrusted inputs to provide better diagnostics and explicit
intent.

### Sandbox: Read-only bypasses

Fields: **Type=Bug**, **Security_Impact-None**

Rationale: Sandbox bypasses that only lead to read access outside of the sandbox are not considered security vulnerabilities for now.

### Breakage through directly invoking internal runtime functions with `%`-syntax

Runtime functions like `%IterableForEach()` are directly visible to JavaScript programs via `--allow-natives-syntax`.
The functions are not supposed to be tested this way, as they generally have pre- and post-conditions.
This can lead to crashes (e.g., [484110302](https://crbug.com/484110302)) when they are incorrectly used.
Such crashes are working as intended.

Functions that are exposed under fuzzing are specified in [`Runtime::IsEnabledForFuzzing()`](https://source.chromium.org/search?q=Runtime::IsEnabledForFuzzing()&ss=chromium).
This function also mentions potential caveats that could still lead to crashes.
To make this clear, V8 will automatically remove any calls to unsupported functions when being invoked with `--fuzzing`.

### Relying on broken snapshots to craft vulnerabilities

V8 uses snapshots that are deserialized at startup.
These snapshots are fully trusted and outside of the attacker model, see the Chrome Security FAQ regarding [physically local attacks](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/faq.md#why-arent-physically_local-attacks-in-chromes-threat-model).

### Bugs reproducing only with `--enable-inspector`

Fields: **component=Platform>DevTools** (componentid:1457055), **Type=Bug**, **Security_Impact-None**

In addition, assignee should be one of the [OWNERS](../../src/inspector/OWNERS).

Rationale: `--enable-inspector` does not accurately represent the production behavior of DevTools.
`inspector-test` should be used for valid reproductions.
For more details see [security documentation](../../src/inspector/SECURITY.md).
