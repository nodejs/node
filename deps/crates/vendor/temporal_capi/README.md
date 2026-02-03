# temporal_capi [![crates.io](https://img.shields.io/crates/v/temporal_capi)](https://crates.io/crates/temporal_capi)

<!-- cargo-rdme start -->

This crate contains the original definitions of [`temporal_rs`] APIs consumed by [Diplomat](https://github.com/rust-diplomat/diplomat)
to generate FFI bindings. We currently generate bindings for C++ and `extern "C"` APIs.

The APIs exposed by this crate are *not intended to be used from Rust* and as such this crate may change its Rust API
across compatible semver versions. In contrast, the `extern "C"` APIs and all the language bindings are stable within
the same major semver version.

[`temporal_rs`]: http://crates.io/crates/temporal_rs

<!-- cargo-rdme end -->
