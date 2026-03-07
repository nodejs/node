# radium

[![Latest Version][version_img]][crate_link]
[![Documentation][docs_img]][docs_link]

`radium` provides abstractions and graceful degradation for behavior that *must*
be shared-mutable, but merely *may* use atomic instructions to do so.

The primary export is the [`Radium`] trait. This is implemented on all symbols
in the [`atomic`] module, and on their [`Cell<T>`] equivalents, and presents the
atomic inherent API as a trait. Your code can be generic over `Radium`, use a
stable and consistent API, and permit callers to select atomic or `Cell`
behavior as they need.

The symbols in the [`atomic`] module are conditionally present according to the
target architecture’s atomic support. As such, code that is portable across
targets with varying atomic support cannot use those names directly. Instead,
the [`radium::types`] module provides names that will always exist, and forward
to the corresponding atomic type when it exists and the equivalent [`Cell<T>`]
type when it does not.

As the `cfg(target_has_atomic)` compiler attribute is unstable, `radium`
provides the macro `radium::if_atomic!` to perform conditional compilation based
on atomic availability.

This crate is `#![no_std]`-compatible, and uses no non-core types.

## Versioning

Each change of supported target architecture will result in a new minor version.
Furthermore, `radium` is by definition attached to the Rust standard library.
As the atomic API evolves, `radium` will follow it. MSRV raising is always at
least a minor-version increase.

If you require a backport of architecture discovery to older Rust versions,
please file an issue. We will happily backport upon request, but we do not
proactively guarantee support for compilers older than ~six months.

## Target Architecture Compatibility

Because the compiler does not expose this information to libraries, `radium`
uses a build script to detect the target architecture and emit its own
directives that mark the presence or absence of an atomic integer. We accomplish
this by reading the compiler’s target information records and copying the
information directly into the build script.

If `radium` does not work for your architecture, please update the build script
to handle your target string and submit a pull request. We write the build
script on an as-needed basis; it is not proactively filled with all of the
information listed in the compiler.

**NOTE**: The build script receives information through two variables: `TARGET`
and `CARGO_CFG_TARGET_ARCH`. The latter is equivalent to the value in
`cfg!(target_arch =)`; however, this value **does not** contain enough
information to fully disambiguate the target. The build script attempts to do
rudimentary parsing of the `env!(TARGET)` string; if this does not work for your
target, consider using the `TARGET_ARCH` matcher, or match on the full `TARGET`
string rather than the parse attempt.

---

**@kneecaw** - <https://twitter.com/kneecaw/status/1132695060812849154>
> Feelin' lazy: Has someone already written a helper trait abstracting
> operations over `AtomicUsize` and `Cell<usize>` for generic code which may
> not care about atomicity?

**@ManishEarth** - <https://twitter.com/ManishEarth/status/1132706585300496384>
> no but call the crate radium
>
> (since people didn't care that it was radioactive and used it in everything)

<!-- Badges -->
[crate_link]: https://crates.io/crates/raidum "Crates.io package"
[docs_img]: https://docs.rs/radium/badge.svg "Radium documentation badge"
[docs_link]: https://docs.rs/radium "Radium documentation"
[version_img]: https://img.shields.io/crates/v/radium.svg "Radium version badge"
[`Cell<T>`]: https://doc.rust-lang.org/core/cell/struct.Cell.html
[`Radium`]: https://docs.rs/radium/latest/radium/trait.Radium.html
[`atomic`]: https://doc.rust-lang.org/core/sync/atomic
[`radium::types`]: https://docs.rs/radium/latest/radium/types
