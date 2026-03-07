// Copyright 2026 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// See comment in `include.rs` for why we disable the prelude.
#![no_implicit_prelude]
#![allow(warnings)]
#![cfg_attr(__ZEROCOPY_INTERNAL_USE_ONLY_NIGHTLY_FEATURES_IN_TESTS, feature(trivial_bounds))]

include!("include.rs");

#[derive(imp::FromBytes)]
#[zerocopy(on_error = "fail")]
#[zerocopy(crate = "zerocopy_renamed")]
struct LoudValid;

util_assert_impl_all!(LoudValid: imp::FromBytes);

// `derive(Unaligned)` fails without a repr.
#[derive(imp::FromBytes, imp::IntoBytes, imp::Unaligned)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
struct Foo {
    a: u8,
}

util_assert_impl_all!(Foo: imp::FromBytes, imp::IntoBytes);
util_assert_not_impl_any!(Foo: imp::Unaligned);

// Invalid enum for FromZeros (must have discriminant 0).
#[derive(imp::FromZeros)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum BadFromZerosEnum {
    A = 1,
    B = 2,
}

util_assert_not_impl_any!(BadFromZerosEnum: imp::FromZeros);

// Invalid enum for FromBytes (must have 256 variants).
#[derive(imp::FromBytes)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum BadFromBytesEnum {
    A = 0,
}

util_assert_not_impl_any!(BadFromBytesEnum: imp::FromBytes);

// Invalid enum for IntoBytes (invalid repr).
#[derive(imp::IntoBytes)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
#[cfg_attr(
    any(
        __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "nightly",
        __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "stable"
    ),
    repr(Rust)
)]
enum BadIntoBytesEnum {
    A,
}

util_assert_not_impl_any!(BadIntoBytesEnum: imp::IntoBytes);

// Invalid enum for Unaligned (invalid repr).
#[derive(imp::Unaligned)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u16)]
enum BadUnalignedEnum {
    A,
}

util_assert_not_impl_any!(BadUnalignedEnum: imp::Unaligned);

// Invalid enum for TryFromBytes (invalid repr).
#[derive(imp::TryFromBytes)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
#[cfg_attr(
    any(
        __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "nightly",
        __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "stable"
    ),
    repr(Rust)
)]
enum BadTryFromBytesEnum {
    A,
}

util_assert_not_impl_any!(BadTryFromBytesEnum: imp::TryFromBytes);

// Invalid union for IntoBytes (invalid repr).
#[derive(imp::IntoBytes)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
#[cfg_attr(
    any(
        __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "nightly",
        __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "stable"
    ),
    repr(Rust)
)]
union BadIntoBytesUnion {
    a: u8,
}

util_assert_not_impl_any!(BadIntoBytesUnion: imp::IntoBytes);

// Invalid union for Unaligned (invalid repr).
#[derive(imp::Unaligned)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
#[cfg_attr(
    any(
        __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "nightly",
        __ZEROCOPY_INTERNAL_USE_ONLY_TOOLCHAIN = "stable"
    ),
    repr(Rust)
)]
union BadUnalignedUnion {
    a: u8,
}

util_assert_not_impl_any!(BadUnalignedUnion: imp::Unaligned);

// Invalid union for IntoBytes (generic).
#[derive(imp::IntoBytes)]
#[zerocopy(on_error = "skip")]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union BadIntoBytesUnionGeneric<T: imp::Copy> {
    a: T,
}

util_assert_not_impl_any!(BadIntoBytesUnionGeneric<u8>: imp::IntoBytes);

#[cfg(__ZEROCOPY_INTERNAL_USE_ONLY_NIGHTLY_FEATURES_IN_TESTS)]
mod trivial_bounds {
    use super::*;

    #[derive(imp::FromBytes)]
    #[zerocopy(on_error = "skip")]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(transparent)]
    struct TrivialBounds(bool);

    util_assert_not_impl_any!(TrivialBounds: imp::FromBytes);

    #[derive(imp::IntoBytes)]
    #[zerocopy(on_error = "skip")]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(C)]
    struct BadIntoBytesStructPadding {
        a: u8,
        b: u16,
    }

    util_assert_not_impl_any!(BadIntoBytesStructPadding: imp::IntoBytes);
}
