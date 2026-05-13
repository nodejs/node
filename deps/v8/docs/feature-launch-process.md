---
title: 'Implementing and shipping JavaScript/WebAssembly language features'
description: 'This document explains the process for implementing and shipping JavaScript or WebAssembly language features in V8.'
---
In general, V8 follows the [Blink Intent process for already-defined consensus-based standards](https://www.chromium.org/blink/launching-features/#process-existing-standard) for JavaScript and WebAssembly language features. V8-specific errata are laid out below. Please follow the Blink Intent process, unless the errata tells you otherwise.

If you have any questions on this topic for JavaScript features, please email syg@chromium.org and v8-dev@googlegroups.com.

For WebAssembly features, please email gdeepti@chromium.org and v8-dev@googlegroups.com.

## Errata

### JavaScript features usually wait until Stage 3+ { #stage3plus }

As a rule of thumb, V8 waits to implement JavaScript feature proposals until they advance to [Stage 3 or later in TC39](https://tc39.es/process-document/). TC39 has its own consensus process, and Stage 3 or later signals explicit consensus among TC39 delegates, including all browser vendors, that a feature proposal is ready to implement. This external consensus process means Stage 3+ features do not need to send Intent emails other than Intent to Ship.

### TAG review { #tag }

For smaller JavaScript or WebAssembly features, a TAG review is not required, as TC39 and the Wasm CG already provide significant technical oversight. If the feature is large or cross-cutting (e.g., requires changes to other Web Platform APIs or modifications to Chromium), TAG review is recommended.

### Both V8 and blink flags are required { #flags }

When implementing a feature, both a V8 flag and a blink `base::Feature` are required.

Blink features are required so that Chrome can turn off features without distributing new binaries in emergency situations. This is usually implemented in [`gin/gin_features.h`](https://source.chromium.org/chromium/chromium/src/+/main:gin/gin_features.h), [`gin/gin_features.cc`](https://source.chromium.org/chromium/chromium/src/+/main:gin/gin_features.cc), and [`gin/v8_initializer.cc`](https://source.chromium.org/chromium/chromium/src/+/main:gin/v8_initializer.cc),

### Fuzzing is required to ship { #fuzzing }

JavaScript and WebAssembly features must be fuzzed for a minimum period of 4 weeks, or one (1) release milestone, with all fuzz bugs fixed, before they can be shipped.

For code-complete JavaScript features, start fuzzing by moving the feature flag to the `JAVASCRIPT_STAGED_FEATURES_BASE` macro in [`src/flags/flag-definitions.h`](https://crsrc.org/c/v8/src/flags/flag-definitions.h).

For WebAssembly, see the [WebAssembly shipping checklist](/docs/wasm-shipping-checklist).

### [Chromestatus](https://chromestatus.com/) and review gates { #chromestatus }

The blink intent process includes a series of review gates that must be approved on the feature's entry in [Chromestatus](https://chromestatus.com/) before an Intent to Ship is sent out seeking API OWNER approvals.

These gates are tailored towards web APIs, and some gates may not be applicable to JavaScript and WebAssembly features. The following is broad guidance. The specifics differ from feature to feature; do not apply guidance blindly!

#### Privacy

Most JavaScript and WebAssembly features do not affect privacy. Rarely, features may add new fingerprinting vectors that reveal information about a user's operating system or hardware.

#### Security

While JavaScript and WebAssembly are common attack vectors in security exploits, most new features do not add additional attack surface. [Fuzzing](#fuzzing) is required, and mitigates some of the risk.

Features that affect known popular attack vectors, such as `ArrayBuffer`s in JavaScript, and features that might enable side-channel attacks, need extra scrutiny and must be reviewed.

#### Enterprise

Throughout their standardization process in TC39 and the Wasm CG, JavaScript and WebAssembly features already undergo heavy backwards compatibility scrutiny. It is exceedingly rare for features to be willfuly backwards incompatible.

For JavaScript, recently shipped features can also be disabled via `chrome://flags/#disable-javascript-harmony-shipping`.

#### Debuggability

JavaScript and WebAssembly features' debuggability differs significantly from feature to feature. JavaScript features that only add new built-in methods do not need additional debugger support, while WebAssembly features that add new capabilities may need significant additional debugger support.

For more details, see the [JavaScript feature debugging checklist](https://docs.google.com/document/d/1_DBgJ9eowJJwZYtY6HdiyrizzWzwXVkG5Kt8s3TccYE/edit#heading=h.u5lyedo73aa9) and the [WebAssembly feature debugging checklist](https://goo.gle/devtools-wasm-checklist).

When in doubt, this gate is applicable.

#### Testing { #tests }

Instead of WPT, Test262 tests are sufficient for JavaScript features, and WebAssembly spec tests are sufficient for WebAssembly features.

Adding Web Platform Tests (WPT) is not required, as JavaScript and WebAssembly language features have their own interoperable test repositories that are run by multiple implementations. Feel free to add some though, if you think it is beneficial.

For JavaScript features, explicit correctness tests in [Test262](https://github.com/tc39/test262) are required. Note that tests in the [staging directory](https://github.com/tc39/test262/blob/main/CONTRIBUTING.md#staging) suffice.

For WebAssembly features, explicit correctness tests in the [WebAssembly Spec Test repo](https://github.com/WebAssembly/spec/tree/main/test) are required.

For performance tests, JavaScript already underlies most existing performance benchmarks, like Speedometer.

### Who to CC { #cc }

**Every** “intent to `$something`” email (e.g. “intent to implement”) should CC <v8-users@googlegroups.com> in addition to <blink-dev@chromium.org>. This way, other embedders of V8 are kept in the loop too.

### Link to the spec repo { #spec }

The Blink Intent process requires an explainer. Instead of writing a new doc, feel free to link to respective spec repository instead (e.g. [`import.meta`](https://github.com/tc39/proposal-import-meta)).
