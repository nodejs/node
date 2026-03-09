<!-- Copyright 2025 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# Validating Changes

This document covers the procedures and requirements for validating changes to
the project, including linting, testing, and pre-submission checks.

## Linting

Clippy should **always** be run on the `nightly` toolchain.

```bash
./cargo.sh +nightly clippy
./cargo.sh +nightly clippy --tests
```

### Strict Linting

- We deny warnings in CI. Even warnings not explicitly listed in `lib.rs` will
  cause CI to fail.
  - **Why:** We maintain a zero-warning policy so that new warnings (which often
    indicate bugs) are immediately obvious and not obscured by existing ones.
- Do not introduce new warnings.
- Respect the strict `deny` list in `src/lib.rs`.

## Validating Changes

Ensure the library builds on all supported toolchains and that Clippy passes.

```bash
./cargo.sh +msrv check --tests --features __internal_use_only_features_that_work_on_stable
./cargo.sh +stable check --tests --features __internal_use_only_features_that_work_on_stable
./cargo.sh +nightly check --tests --all-features
./cargo.sh +nightly clippy --tests --all-features --workspace
```

**Note:** Tests are rarely toolchain-sensitive. Running tests on `nightly` is
usually sufficient.

## Testing Strategy

- **Unit Tests:** Place unit tests in a `mod tests` module within the source
  file they test.
- **UI/Compile-Fail Tests:**
    - **`zerocopy`:** Place in `tests/ui-*` (top-level). The top-level `tests`
      directory contains *only* UI tests.
    - **`zerocopy-derive`:** Place in `zerocopy-derive/tests/ui-*`.
- **Derive Integration Tests:** Place integration tests for derive macros in
  `zerocopy-derive/tests`.
- **Derive Output Tests:** Place unit tests that verify the *generated code*
  (token streams) in `zerocopy-derive/src/output_tests.rs`.
- **Formal Verification (Kani):** Place Kani proofs in a `mod proofs` module
  within the source file they test.
    - **Purpose:** Use the
      [Kani Rust Verifier](https://model-checking.github.io/kani/) to prove the
      soundness of `unsafe` code or code relied upon by `unsafe` blocks. Unlike
      testing, which checks specific inputs, Kani proves properties for *all*
      possible inputs.
    - **How to Write Proofs:**
        - **Harnesses:** Mark proof functions with `#[kani::proof]`.
        - **Inputs:** Use `kani::any()` to generate arbitrary inputs.
        - **Assumptions:** Use `kani::assume(condition)` to constrain inputs to
          valid states (e.g., `align.is_power_of_two()`).
        - **Assertions:** Use `assert!(condition)` to verify the properties you
          want to prove.
    - **CI:** Kani runs in CI using the `model-checking/kani-github-action` with
      specific feature flags to ensure compatibility.

<!-- FIXME: Describe how to ensure that a Kani proof is "total" (esp wrt function inputs). -->

## Feature Gates

When editing code gated by a feature, compile **with and without** that feature.

```bash
./cargo.sh +stable check --tests
./cargo.sh +stable check --tests --feature foo
```
