<!-- Copyright 2025 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# Development Guidelines

This document covers guidelines for developing code changes.

## Build and Test

This repository uses a wrapper script (`cargo.sh`) to ensure consistent
toolchain usage and configuration.

> [!IMPORTANT]
> **NEVER** run `cargo` directly.
> **ALWAYS** use `./cargo.sh` for all cargo sub-commands.
>
> **Why?** `cargo.sh` ensures that the toolchains used in development match
> those in CI, which is important because some features are only available on
> specific toolchains, and because UI tests rely on the text of compiler errors,
> which changes between toolchain versions.

### Syntax

`./cargo.sh +<toolchain> <command> [args]`

This is equivalent to:

`cargo +1.2.3 <command> [args]`

...where `1.2.3` is the toolchain version named by `<toolchain>` (e.g., `msrv` ->
`1.56.0`).

### Toolchains

The `<toolchain>` argument is mandatory:
- `msrv`: Minimum Supported Rust Version.
- `stable`: Stable toolchain.
- `nightly`: Nightly toolchain.
- `all`: Runs on `msrv`, `stable`, and `nightly` sequentially.
- Version-gated: e.g., `no-zerocopy-core-error-1-81-0` (see `Cargo.toml`).


## MSRV (Minimum Supported Rust Version)

The MSRV is **1.56.0**.

- **Do NOT** use features stabilized after 1.56.0 unless version-gated.
- **Requirement:** Ask for user approval before introducing new version-gated
  behavior.
- **Verify**: Ensure code compiles on 1.56.0 (`./cargo.sh +msrv ...`).

### Version Gating Convention

We use `[package.metadata.build-rs]` in `Cargo.toml` to gate features by Rust version.

1.  **Define**: Add `no-zerocopy-<feature>-<version> = "<version>"` to `Cargo.toml`.
2.  **Use**: Use `#[cfg(not(no_zerocopy_<feature>_<version>))]` (note underscores).
3.  **Document**: For public items, use `#[cfg_attr(doc_cfg, doc(cfg(rust = "<version>")))]`.

**Important:** The toolchains listed in `.github/workflows/ci.yml` and
`Cargo.toml` (under `[package.metadata.build-rs]`) must be kept in sync. If you
add a new version-gated feature, ensure it is reflected in both places.

## UI Tests

For advice on how to add, modify, or remove UI tests (in `tests/ui-*` or
`zerocopy-derive/tests/ui-*`), refer to [agent_docs/ui_tests.md](./ui_tests.md).

## Macro Development

- **Shared Logic:** Put shared macro logic in `src/util/macro_util.rs` to avoid
  code duplication in generated code.
- **Lints:** Generated code often triggers lints. Use `#[allow(...)]` liberally
  in generated code to suppress them.
  ```rust
  // GOOD: Suppress lints that might be triggered by generated names.
  // Example: Using a variant name (PascalCase) as a field name (snake_case).
  // Input: `enum MyEnum { VariantA }`
  // Generated: `union Variants { __field_VariantA: ... }`
  quote! {
      #[allow(non_snake_case)]
      union ___ZerocopyVariants {
          #(#fields)*
      }
  }
  ```

## Unsafe Code

`unsafe` code is extremely dangerous and should be avoided unless absolutely
necessary. For guidelines on writing unsafe code, including pointer casts and
safety comments, refer to [agent_docs/unsafe_code.md](./unsafe_code.md).
