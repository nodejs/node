[![github-img]][github-url] [![crates-img]][crates-url] [![docs-img]][docs-url]

Checked owned and borrowed strings.

# Overview

The Rust standard library provides the `String` and `str` types, which wrap
`Vec<u8>` and `[u8]` respectively, with the invariant that the contents
are valid UTF-8.

This crate abstracts the idea of type-level invariants on strings by
introducing the immutable `Check` and `Ck` types, where the invariants
are determined by a generic `Invariant` type parameter. It offers
`UnicodeIdent` and `RustIdent` `Invariant`s, which are enabled by the
`ident` feature flag.

"strck" comes from "str check", similar to how rustc has typeck and
borrowck for type check and borrow check respectively.

See [the documentation][docs-url] for more details.

# Motivation

Libraries working with string-like types with certain properties, like identifiers,
quickly become confusing as `&str` and `String` begin to pollute type signatures
everywhere. One solution is to manually implement an owned checked string type
like `syn::Ident` to disambiguate the type signatures and validate the string.
The downside is that new values cannot be created without allocation,
which is unnecessary when only a borrowed version is required.

`strck` solves this issue by providing a checked borrowed string type, `Ck`,
alongside a checked owned string type, `Check`. These serve as thin wrappers
around `str` and `String`[^1] respectively, and prove at the type level that
the contents satisfy the `Invariant` that the wrapper is generic over.

[^1]: `Check` can actually be backed by any `'static + AsRef<str>` type,
but `String` is the default.

# Use cases

### Checked strings without allocating

The main benefit `strck` offers is validating borrowed strings via the
`Ck` type without having to allocate in the result.

```rust
use strck::{Ck, IntoCk, ident::rust::RustIdent};

let this_ident: &Ck<RustIdent> = "this".ck().unwrap();
```

### Checked zero-copy deserialization

When the `serde` feature flag is enabled, `Ck`s can be used to perform
checked zero-copy deserialization, which requires the
`#[serde(borrow)]` attribute.

```rust
use strck::{Ck, ident::unicode::UnicodeIdent};

#[derive(Serialize, Deserialize)]
struct Player<'a> {
    #[serde(borrow)]
    username: &'a Ck<UnicodeIdent>,
    level: u32,
}
```

Note that this code sample explicitly uses `Ck<UnicodeIdent>` to demonstrate
that the type is a `Ck`. However, `strck` provides `Ident` as an
alias for `Ck<UnicodeIdent>`, which should be used in practice.


### Infallible parsing

For types where string validation is relatively cheap but parsing is costly
and fallible, `strck` can be used with a custom `Invariant` as an input to
make an infallible parsing function.

# Postfix construction with `IntoCk` and `IntoCheck`

This crate exposes two helper traits, `IntoCk` and `IntoCheck`. When in
scope, the `.ck()` and `.check()` functions can be used to create
`Ck`s and `Check`s respectively:

```rust
use strck::{IntoCheck, IntoCk, ident::unicode::UnicodeIdent};

let this_ident = "this".ck::<UnicodeIdent>().unwrap();
let this_foo_ident = format!("{}_foo", this_ident).check::<UnicodeIdent>().unwrap();
```

# Documentation

See the [crate-level documentation][docs-url] for more details.

[github-url]: https://github.com/QnnOkabayashi/strck
[crates-url]: https://crates.io/crates/strck
[docs-url]: https://docs.rs/strck
[github-img]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
[crates-img]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
[docs-img]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
