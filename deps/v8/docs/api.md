---
title: 'V8’s public API'
description: 'This document discusses the stability of V8’s public API, and how developers can make changes to it.'
---
This document discusses the stability of V8’s public API, and how developers can make changes to it.

## API stability

If V8 in a Chromium canary turns out to be crashy, it gets rolled back to the V8 version of the previous canary. It is therefore important to keep V8’s API compatible from one canary version to the next.

We continuously run a [bot](https://ci.chromium.org/p/v8/builders/luci.v8.ci/Linux%20V8%20API%20Stability) that signals API stability violations. It compiles Chromium’s HEAD with V8’s [current canary version](https://chromium.googlesource.com/v8/v8/+/refs/heads/canary).

Failures of this bot are currently only FYI and no action is required. The blame list can be used to easily identify dependent CLs in case of a rollback.

If you break this bot, be reminded to increase the window between a V8 change and a dependent Chromium change next time.

## How to change V8’s public API

V8 is used by many different embedders: Chrome, Node.js, gjstest, etc. When changing V8’s public API (basically the files under the `include/` directory) we need to ensure that the embedders can smoothly update to the new V8 version. In particular, we cannot assume that an embedder updates to the new V8 version and adjusts their code to the new API in one atomic change.

The embedder should be able to adjust their code to the new API while still using the previous version of V8. All instructions below follow from this rule.

- Adding new types, constants, and functions is safe with one caveat: do not add a new pure virtual function to an existing class. New virtual functions should have default implementation.
- Adding a new parameter to a function is safe if the parameter has the default value.
- Removing or renaming types, constants, functions is unsafe. Use the [`V8_DEPRECATED`](https://crsrc.org/c/v8/include/v8config.h?q=V8_DEPRECATED) and [`V8_DEPRECATE_SOON`](https://crsrc.org/c/v8/include/v8config.h?q=V8_DEPRECATE_SOON) macros, which causes compile-time warnings when the deprecated methods are called by the embedder. For example, let’s say we want to rename function `foo` to function `bar`. Then we need to do the following:
    - Add the new function `bar` near the existing function `foo`.
    - Wait until the CL rolls in Chrome. Adjust Chrome to use `bar`.
    - Annotate `foo` with `V8_DEPRECATED("Use bar instead") void foo();`
    - In the same CL adjust the tests that use `foo` to use `bar`.
    - Write in CL motivation for the change and high-level update instructions.
    - Wait until the next V8 branch.
    - Remove function `foo`.

    `V8_DEPRECATE_SOON` is a softer version of `V8_DEPRECATED`. Chrome will not break with it, so step b is not need. `V8_DEPRECATE_SOON` is not sufficient for removing the function.

    You still need to annotate with `V8_DEPRECATED` and wait for the next branch before removing the function.

    `V8_DEPRECATED` can be tested using the `v8_deprecation_warnings` GN flag.
    `V8_DEPRECATE_SOON` can be tested using `v8_imminent_deprecation_warnings`.

- Changing function signatures is unsafe. Use the `V8_DEPRECATED` and `V8_DEPRECATE_SOON` macros as described above.

We maintain a [document mentioning important API changes](https://docs.google.com/document/d/1g8JFi8T_oAE_7uAri7Njtig7fKaPDfotU6huOa1alds/edit) for each V8 version.

There is also a regularly updated [doxygen api documentation](https://v8.dev/api).
