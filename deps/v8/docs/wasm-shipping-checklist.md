---
title: 'Checklist for staging, experimenting with, and shipping WebAssembly features'
description: 'This document provides checklists of engineering requirements on when to stage, experiment with, and ship a WebAssembly feature in V8.'
---
This document provides checklists of engineering requirements for staging, experimenting with, and shipping WebAssembly features in V8. These checklists are meant as a guideline and may not be applicable to all features. The actual launch process is described in the [V8 Launch process](https://v8.dev/docs/feature-launch-process).

# Overview

A feature can be anything from a visible addition to the WebAssembly API which is driven by a W3C WebAssembly community group proposal to a larger architectural change that improves performance, stability or user experience.

For W3C WebAssembly proposals, we always follow this process even if the proposal is comparably small. In that case, the trials can be skipped if there is enough confidence in the design. But all other requirements are mandatory. For non-proposals, the application of this process depends on the complexity and the risk associated with it. E.g., a simple compiler optimization would not require going through the steps while adding a new compiler all together certainly would. As a rule of thumb, if a feature is complex enough to require adding a feature flag during development, then it's likely worth following this process. If an optimization can be merged in a few CLs during one milestone development phase, it's small enough to ship directly.

Features of this complexity start off behind an experimental flag which enables the feature for developers that would like to try it out and provide feedback and allows us to test the feature in a limited capacity. As these features require explicit command line arguments, we don't expect users to enable them and if they do, it's at their own risk.

Once we consider a feature sufficiently stable that we consider experimentation or even shipping, we (pre-)stage it. This enables the feature on our fuzzers, test and benchmarking infrastructure and allows us to detect issues early on. Once it has shown to be sufficiently stable (usually after ~2 weeks without major incidents), we open it to the [Vulnerability Reward Program (VRP)](https://bughunters.google.com/about/rules/5745167867576320/chrome-vulnerability-reward-program-rules) to allow external security researchers to test it too and file bugs on it.

Some features might ship directly from this phase, if we don't expect to gain any insights from further experimentation. Others will go through one or more phases of experimentation, e.g. developer trial, origin trial or Finch trial where we collect data from partners or in-the-wild usage.

An overview over the shipping phases together with their respective requirements is shown here:

![Overview of WebAssembly shipping phases](/_img/wasm-launch-process/phases.svg)


## Flags

We usually define one or more command line flags that guard the feature from being active in production environments before it's ready for general use. These flags allow fine-grained control for testing and debugging and can be kept beyond the release of a feature to switch it off when needed. This is mostly not necessary and not worth maintaining the alternative code path, but can sometimes be useful (e.g. we kept the flags for lazy compilation and dynamic tiering).

### Wasm feature flags vs. V8 flags

In WebAssembly, we have the option of using a Wasm feature flag (`--experimental-wasm-*`) which is defined via a macro in [`src/wasm/wasm-feature-flags.h`](https://cs.chromium.org/chromium/src/v8/src/wasm/wasm-feature-flags.h) (different macros for different phases of development). These flags are usually used for new functionality, e.g. related to a new WebAssembly proposal.

Alternatively, one can use a regular V8 flag as defined in [`src/flags/flag-definitions.h`](https://cs.chromium.org/chromium/src/v8/src/flags/flag-definitions.h). These flags are commonly used for architectural changes or optimizations. In early stages, you should use `DEFINE_EXPERIMENTAL_FEATURE()`.

### Flags for (pre-)staging

There are also common flags which bundle multiple experimental flags together through implications. `--experimental-fuzzing` is for enabling experimental features on our fuzzers in the pre-staging phase. Wasm feature flags defined in the `FOREACH_WASM_PRE_STAGING_FEATURE_FLAG` macro are automatically implied by this flag. V8 flags for pre-staged features require an explicit implication in [`src/flags/flag-definitions.h`](https://cs.chromium.org/chromium/src/v8/src/flags/flag-definitions.h).

Wasm feature flags also require a use counter to be added (or explicitly disabled this using `kIntentionallyNoUseCounter`). It's generally advisable to add a use counter to track adoption. You can pick a `WebFeature` or a `WebDXFeature` for your implementation. If it's linked to a W3C WebAssembly proposal, `WebDXFeature` is recommended. Otherwise, a `WebFeature` can be used which requires no approval process.

For staged features, that are ready for public evaluation (including the VRP) before their launch, we have the `--wasm-staging` flag which implies all Wasm feature flags defined in the `FOREACH_WASM_STAGING_FEATURE_FLAG` and covers new functionality about to be launched in the near future. For features that are non-functional like optimizations, one can add an explicit implication from `--future`. This flag is also used for benchmarking the performance of upcoming V8 versions.

## Phases

### Inception

This is the phase in which implementation in V8 is starting, but there might not be a [Chrome feature entry](https://chromestatus.com/features) or even a proper name for the feature. Code might be in local branches only or submitted to the main branch, guarded behind a feature flag.

### Developer trial (optional)

We can optionally ask external partners for feedback on the scope, interface or performance of the feature. During the developer trial, they can only test locally, because enabling the feature requires explicitly enabling the feature flag via the command line. A developer trial may start before staging and can continue until shipping.

### (Pre-)Staged

Once we believe the feature is mature enough to consider user testing or even shipping, we stage it for at least one milestone. This increases coverage on our test and fuzzing infrastructure. The pre-staging phase is enabled by adding the feature flag as an implication to `--experimental-fuzzing`.

After a short time in this stage, we will move the implication to `--wasm-staging` or `--future` depending on whether it's a feature or an optimization/architectural change respectively. This will open it for the VRP to encourage external researchers to find issues with the code. During this phase, we usually hold a shipping review where the development team assesses the test and fuzzer coverage and decides on requirements for the following phases.

### Origin/field trial

If we need more data to decide on the readiness of a feature, we can schedule a trial. This can either be an origin trial in tight collaboration with partners or a broader field trial (Finch). Origin trials tend to run for longer than field trials, but complex features might also spend several months in a field trial until they are sufficiently mature.

### Shipped

Once a feature is stable, complete and fully spec'd (phase 4 in the WebAssembly Community Group), we can ship it. This enables the feature for all users, even though only a tiny fraction of websites might use it in the beginning. We keep the flag around for 1-2 more milestones to be able to switch the feature off in case of unexpected side-effects.

### Clean up

After 1-2 milestones, we can remove the flag, outdated code and do other clean-up work. For some features, it might be worth keeping the flag around to allow easier debugging, A/B comparisons, etc.


# Staging

## When to stage a WebAssembly feature

The [staging](https://docs.google.com/document/d/1ZgyNx7iLtRByBtbYi1GssWGefXXciLeADZBR_FxG-hE) of a WebAssembly feature defines the end of its implementation phase. The implementation phase is finished when the following checklist is done:

- The implementation in V8 is complete. This includes:
    - Implementation in Turbofan/Turboshaft (if applicable)
    - Implementation in Liftoff (if applicable)
    - Basic fuzzer coverage (if applicable)
- Tests in V8 are available.
- Spec tests are rolled into V8 by running [`tools/wasm/update-wasm-spec-tests.sh`](https://cs.chromium.org/chromium/src/v8/tools/wasm/update-wasm-spec-tests.sh).
- All existing proposal spec tests pass. Missing spec tests are unfortunate but should not block staging.

Note that the stage of the feature proposal in the standardization process does not matter for staging the feature in V8. The proposal should, however, be mostly stable.

## How to stage a WebAssembly feature

### Staging Wasm feature flags

Pre-stage the feature to collect fuzzer coverage for two weeks

- In [`src/wasm/wasm-feature-flags.h`](https://cs.chromium.org/chromium/src/v8/src/wasm/wasm-feature-flags.h), move the feature flag from the `FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG` macro list to the `FOREACH_WASM_PRE_STAGING_FEATURE_FLAG` macro list.
- In [`tools/wasm/update-wasm-spec-tests.sh`](https://cs.chromium.org/chromium/src/v8/tools/wasm/update-wasm-spec-tests.sh), add the proposal repository name to the `repos` list of repositories.
- Run [`tools/wasm/update-wasm-spec-tests.sh`](https://cs.chromium.org/chromium/src/v8/tools/wasm/update-wasm-spec-tests.sh) to create and upload the spec tests of the new proposal.
- In [`test/wasm-spec-tests/testcfg.py`](https://cs.chromium.org/chromium/src/v8/test/wasm-spec-tests/testcfg.py), add the proposal repository name and the feature flag to the `proposal_flags` list.
- In [`test/wasm-js/testcfg.py`](https://cs.chromium.org/chromium/src/v8/test/wasm-js/testcfg.py), add the proposal repository name and the feature flag to the `proposal_flags` list.

After two weeks of fuzzer coverage, we can open the feature to the VRP to encourage external bug reporting.

- In [`src/wasm/wasm-feature-flags.h`](https://cs.chromium.org/chromium/src/v8/src/wasm/wasm-feature-flags.h), move the feature flag from the `FOREACH_WASM_PRE_STAGING_FEATURE_FLAG` macro list to the `FOREACH_WASM_STAGING_FEATURE_FLAG` macro list.

### Staging other feature flags

Pre-stage the feature to collect fuzzer coverage for two weeks

- In [`src/flags/flag-definitions.h`](https://cs.chromium.org/chromium/src/v8/src/flags/flag-definitions.h) add an implication from `experimental_fuzzing` to the feature flag using `DEFINE_WEAK_IMPLICATION()`.

After two weeks of fuzzer coverage, we can open the feature to the VRP to encourage external bug reporting.

- Switch the flag definition from `DEFINE_EXPERIMENTAL_FEATURE` to `DEFINE_BOOL` with a `false` default.
- In [`src/flags/flag-definitions.h`](https://cs.chromium.org/chromium/src/v8/src/flags/flag-definitions.h), move the feature flag implication from the `experimental_fuzzing` to `future` (performance optimizations) or `wasm_staging` (other architectural changes). Either implication will continue fuzzing coverage, but an implication from `future` will also enable it for benchmarking which might or might not be desired.


# Experimentation (optional)

There are multiple ways of experimenting with a new feature and gathering information on its stability and viability. The successful completion of the staging phase ensures that our users are not exposed to experimental code that might be harmful to them. However, full stability is not always guaranteed which is why such experimentation must be executed with great care.

## Developer trial

This is the easiest trial to run. It often does not require any changes to the code, but developers are encouraged to try it out. This can happen via the existing command line flag, by adding a Chrome flag that developers can enable via the `chrome://flags` or by staging a Wasm feature flag which automatically adds it to the existing *Experimental WebAssembly* option there (`chrome://flags#enable-experimental-webassembly-features`). Because the latter option might be switched on by users accidentally (e.g. because they tried another feature earlier and forgot to disable it afterwards), the bar for adding features there is higher and one should carefully evaluate if the feature meets the criteria for staging before choosing this option.

### Steps to enable a developer trial

- Reach out to partners and collect feedback (direct communication, issues or polls).

## Origin trial

Features that web developers want to try out with their own users are ideal for an origin trial. This is often a new WebAssembly proposal that requires feedback from real-world scenarios to evaluate its shape and potential readiness for publication. Developers can set up their own trials where they compare different populations that have the feature enabled or disabled. Sometimes, even different versions of an API can be compared against each other.

The feedback can be collected from partners or via Chrome's metric collection. It is usually reported back to the W3C WebAssembly community group and to the Blink API owners.

### Steps to launch an origin trial

To get the experiment going, do the following

- Inform the Chrome Security Team about the pending experiment (tracking sheet or email).
- Request all required reviews for experimentation on the Chrome Feature entry.
- Send Intent to Experiment (up to 6 months/milestones) to Blink API Owners and get one LGTM.
- Inform the OT team and wait for the resolution.
- Distribute the signup link to interested partners.

To get an extension (up to 3 months/milestones)

- Summarize feedback of the experiment so far.
- Motivate extension and summarize progress in an Intent to Extend Experiment to the Blink API Owners and get one LGTM.
- Update Chrome Status entry and wait for its resolution.
- Ask partners to update their tokens.

## Finch trial

When a feature does not require any changes to user code, Chrome can decide to run a trial without partner engagement. Such trials are ideal for performance improvements or larger architectural changes. Chrome's metric collection can then be used to compare different configurations and their impact on common performance and stability metrics.

### Steps to launch a Finch trial

- Make the Chrome Security Team aware of the pending experiment (tracking sheet or email).
- Consider adding GWS ids and inform partners of the experiment to track any changes in application metrics that are not covered by Chrome (e.g. performance metrics).
- Submit a configuration to be tested in the Chrome repo.
- Enable the Finch experiment, starting with 50% of dev users.
- Regularly check metrics and follow up on alerts.
- After at least 2 weeks of stable experimentation, advance the experiment to 50% of beta users.
- After at least 2 weeks of stable experimentation, advance the experiment to 1% of stable users.
- After at least 2 weeks of stable experimentation, advance the experiment to 10% of stable users.
- After at least 4 weeks of stable experimentation, advance the experiment to 50% of stable users (in case *WebView* is not part of the trial, one can jump straight to shipping from here, but it's recommended to include WebView into each trial).
- After at least 2 weeks of stable experimentation, you can consider shipping.

The longer experimentation time at 10% of stable users is to accommodate for manually detected bugs and reporting which tend to have a longer lead time than signals gathered from metrics and automated testing. At 10% the impact of the experiment is still limited while providing good visibility for partners to identify issues.


# Shipping

## When is a WebAssembly feature ready to be shipped?

- The [V8 Launch process](https://v8.dev/docs/feature-launch-process) is satisfied.
- The implementation is covered by a fuzzer (if applicable).
- The feature has been staged and opened to the VRP for several weeks to get fuzzer coverage and feedback.
- The feature proposal is [stage 4](https://github.com/WebAssembly/proposals) (if applicable).
- All [spec tests](https://github.com/WebAssembly/spec/tree/main/test) pass.
- The [Chromium DevTools checklist for new WebAssembly features](https://docs.google.com/document/d/1WbL-fGuLbbNr5-n_nRGo_ILqZFnh5ZjRSUcDTT3yI8s/preview) is satisfied.

## How to ship a WebAssembly feature

### Prerequisites

- Request all required reviews for shipping on the Chrome Feature entry.
- Send Intent to Ship to Blink API Owners and get three LGTMs.

### Ship Wasm feature flags

- In [`src/wasm/wasm-feature-flags.h`](https://crsrc.org/c/v8/src/wasm/wasm-feature-flags.h), move the feature flag from the `FOREACH_WASM_STAGING_FEATURE_FLAG` macro list to the `FOREACH_WASM_SHIPPED_FEATURE_FLAG` macro list.
- Additionally, enable the feature by default by changing the third parameter in `FOREACH_WASM_SHIPPED_FEATURE_FLAG` to `true`.

### Ship other feature flags

- In [`src/flags/flag-definitions.h`](https://cs.chromium.org/chromium/src/v8/src/flags/flag-definitions.h), remove any implication from `future` and `wasm-staging`.
- Set the default value of the feature in [`src/flags/flag-definitions.h`](https://cs.chromium.org/chromium/src/v8/src/flags/flag-definitions.h) to `true`.

### After enabling the feature

- Ensure to add a blink CQ bot on the CL to check for [blink web test](https://v8.dev/docs/blink-layout-tests) failures caused by enabling the feature (add this line to the footer of the CL description: `Cq-Include-Trybots: luci.v8.try:v8_linux_blink_rel`).
- If the feature has been tried in a Finch experiment, you can soft-launch the feature via Finch by setting its experiment to 100% of users. This allows faster shipping and can be rolled back easily.
- Set a reminder to remove the feature flag, the Finch configuration and outdated code after two milestones.

### Disabling an already shipped feature

If there are any issues during early stages, a *reverse Finch trial* can disable the feature if the flag has not been removed yet and the Finch config is still there. After a prolonged time, this might not be a viable option anymore even if the feature flag is still active, because the alternative code path is no longer tested.
