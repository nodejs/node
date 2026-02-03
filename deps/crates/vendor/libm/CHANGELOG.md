# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.15](https://github.com/rust-lang/compiler-builtins/compare/libm-v0.2.14...libm-v0.2.15) - 2025-05-06

### Other

- Require `target_has_atomic = "ptr"` for runtime feature detection

## [0.2.14](https://github.com/rust-lang/compiler-builtins/compare/libm-v0.2.13...libm-v0.2.14) - 2025-05-03

### Other

- Use runtime feature detection for fma routines on x86

## [0.2.13](https://github.com/rust-lang/compiler-builtins/compare/libm-v0.2.12...libm-v0.2.13) - 2025-04-21

### Fixed

- Switch back to workspace resolver v2 to unbreak builds without the 2024 edition

## [0.2.12](https://github.com/rust-lang/compiler-builtins/compare/libm-v0.2.11...libm-v0.2.12) - 2025-04-21

- Mark generic functions `#[inline]`
- Combine the source files for `fmod`
- Ensure all public functions are marked `no_panic`
- Add assembly version of simple operations on aarch64
- Add `roundeven{,f,f16,f128}`
- Add `fminimum`, `fmaximum`, `fminimum_num`, and `fmaximum_num`
- Eliminate the use of `force_eval!` in `ceil`, `floor`, and `trunc`
- Port the CORE-MATH version of `cbrt`
- Add `fmaf128`
- fma: Ensure zero has the correct sign
- Add `scalbnf16`, `scalbnf128`, `ldexpf16`, and `ldexpf128`
- Specify license as just MIT
- Add `fmodf128`
- Add `fmodf16` using the generic implementation
- Add `fminf16`, `fmaxf16`, `fminf128`, and `fmaxf128`
- Add `roundf16` and `roundf128`
- Add `rintf16` and `rintf128`
- Add `floorf16` and `floorf128`
- Add `ceilf16` and `ceilf128`
- Add `sqrtf16` and `sqrtf128`
- Simplify and optimize `fdim` ([#442](https://github.com/rust-lang/libm/pull/442))
- Add `fdimf16` and `fdimf128`
- Add `truncf16` and `truncf128`
- Add `fabsf16`, `fabsf128`, `copysignf16`, and `copysignf128`
- Move some numeric trait logic to default implementations
- Add some more basic docstrings ([#352](https://github.com/rust-lang/libm/pull/352))
- Add support for loongarch64-unknown-linux-gnu
- Add an "arch" Cargo feature that is on by default
- Rename the `special_case` module to `precision` and move default ULP
- Move the existing "unstable" feature to "unstable-intrinsics"

There are a number of things that changed internally, see the git log for a full
list of changes.

## [0.2.11](https://github.com/rust-lang/libm/compare/libm-v0.2.10...libm-v0.2.11) - 2024-10-28

### Fixed

- fix type of constants in ported sincosf ([#331](https://github.com/rust-lang/libm/pull/331))

### Other

- Disable a unit test that is failing on i586
- Add a procedural macro for expanding all function signatures
- Introduce `musl-math-sys` for bindings to musl math symbols
- Add basic docstrings to some functions ([#337](https://github.com/rust-lang/libm/pull/337))

## [0.2.10](https://github.com/rust-lang/libm/compare/libm-v0.2.9...libm-v0.2.10) - 2024-10-28

### Other

- Set the MSRV to 1.63 and test this in CI

## [0.2.9](https://github.com/rust-lang/libm/compare/libm-v0.2.8...libm-v0.2.9) - 2024-10-26

### Fixed

- Update exponent calculations in nextafter to match musl

### Changed

- Update licensing to MIT AND (MIT OR Apache-2.0), as this is derivative from
  MIT-licensed musl.
- Set edition to 2021 for all crates
- Upgrade all dependencies

### Other

- Don't deny warnings in lib.rs
- Rename the `musl-bitwise-tests` feature to `test-musl-serialized`
- Rename the `musl-reference-tests` feature to `musl-bitwise-tests`
- Move `musl-reference-tests` to a new `libm-test` crate
- Add a `force-soft-floats` feature to prevent using any intrinsics or
  arch-specific code
- Deny warnings in CI
- Fix `clippy::deprecated_cfg_attr` on compiler_builtins
- Corrected English typos
- Remove unneeded `extern core` in `tgamma`
- Allow internal_features lint when building with "unstable"

## [v0.2.1] - 2019-11-22

### Fixed

- sincosf

## [v0.2.0] - 2019-10-18

### Added

- Benchmarks
- signum
- remainder
- remainderf
- nextafter
- nextafterf

### Fixed

- Rounding to negative zero
- Overflows in rem_pio2 and remquo
- Overflows in fma
- sincosf

### Removed

- F32Ext and F64Ext traits

## [v0.1.4] - 2019-06-12

### Fixed

- Restored compatibility with Rust 1.31.0

## [v0.1.3] - 2019-05-14

### Added

- minf
- fmin
- fmaxf
- fmax

## [v0.1.2] - 2018-07-18

### Added

- acosf
- asin
- asinf
- atan
- atan2
- atan2f
- atanf
- cos
- cosf
- cosh
- coshf
- exp2
- expm1
- expm1f
- expo2
- fmaf
- pow
- sin
- sinf
- sinh
- sinhf
- tan
- tanf
- tanh
- tanhf

## [v0.1.1] - 2018-07-14

### Added

- acos
- acosf
- asin
- asinf
- atanf
- cbrt
- cbrtf
- ceil
- ceilf
- cosf
- exp
- exp2
- exp2f
- expm1
- expm1f
- fdim
- fdimf
- floorf
- fma
- fmod
- log
- log2
- log10
- log10f
- log1p
- log1pf
- log2f
- roundf
- sinf
- tanf

## v0.1.0 - 2018-07-13

- Initial release

[Unreleased]: https://github.com/japaric/libm/compare/v0.2.1...HEAD
[v0.2.1]: https://github.com/japaric/libm/compare/0.2.0...v0.2.1
[v0.2.0]: https://github.com/japaric/libm/compare/0.1.4...v0.2.0
[v0.1.4]: https://github.com/japaric/libm/compare/0.1.3...v0.1.4
[v0.1.3]: https://github.com/japaric/libm/compare/v0.1.2...0.1.3
[v0.1.2]: https://github.com/japaric/libm/compare/v0.1.1...v0.1.2
[v0.1.1]: https://github.com/japaric/libm/compare/v0.1.0...v0.1.1
