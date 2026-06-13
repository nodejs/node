# Security Bug Triaging

V8 generally triages security bugs based on [Chromium's guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/security-labels.md).

Google-only: The internal version of this document is available at [go/v8-security-gardening](http://goto.google.com/v8-security-gardening).

## Labels and classifications

- **Type=Vulnerability**: Designates a security vulnerability that impacts users.
- **Severity**: Same as [Chromium's severities](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/severity-guidelines.md).
- **Priority**: A priority that in general is at least the severity.
In certain circumstances, e.g. in-the-wild exploitation, we may raise the priority further.
- **Security_Impact-{Head, Beta, Stable, Extended, None}** hotlists: Derived from milestones set in the **Found In** field, this hotlist specifies the earliest affected release channel.
Should not normally be set by humans, except in the case of **Security_Impact-None** (hotlistid: 5433277) which means that the bug is in a disabled feature, or otherwise doesn't impact Chrome: see the section below for more details.

In addition, the following fields are set as part of triaging
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

### Regular security bugs

Bugs should reproduce on `d8` with `--fuzzing` and `--disallow-unsafe-flags`.
Bugs that only reproduce with other flag combinations have a much higher chance of not being considered security bugs in first place.
See the section below for common scenarios that lead to reclassifications.

### Sandbox security bugs

Bugs should reproduce in the [sandbox testing environment](../src/sandbox/README.md#testing).

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
Flags are often following the naming of `--experimental-*` and imply the `--experimental` flag.
Sometimes these flags also have some experimental annotation on the flag descriptions.

Note: If the flag is part of  e.g. `--future` or `--wasm-staging` then this signals that the flags are ready for fuzzing.
We don’t change the flag names in this case to avoid further churn on the code base.

### Bugs in developer flags such as `--trace-*` or flags that are clearly marked as unsafe

Fields: **Type=Bug**, **Security_Impact-None**

Rationale: Not reachable in production as these flags are only used by developers.

## Other common cases

### `nullptr` (or close to `0`) deref

Fields: **Type=Bug**, **Security_Impact-None**

V8 relies on `nullptr` dereferences to deterministically crash.

### Broken `DCHECK`s or reliable `CHECK` crashers

Fields: **Type=Bug**, **Security_Impact-None**

Rationale: Crashes are either bogus and do not happen in production builds or are deterministically crashing the process.

Note: `CHECK`s must not be behind special builds or phases, such as `--verify-*`.

### Breakage through directly invoking internal runtime functions with `%`-syntax

Runtime functions like `%IterableForEach()` are directly visible to JavaScript programs via `--allow-natives-syntax`.
The functions are not supposed to be tested this way, as they generally have pre- and post-conditions.
This can lead to crashes (e.g. [484110302](crbug.com/484110302)) when they are incorrectly used.
Such crashes are working as intended.

Functions that are exposed under fuzzing are specified in [`Runtime::IsEnabledForFuzzing()`](https://source.chromium.org/search?q=Runtime::IsEnabledForFuzzing()&ss=chromium).
The bottleneck also mentions potential caveats that could still lead to crashes.
To make this clear V8 will automatically remove any calls to unsupported functions when being invoked with `--fuzzing`.
