// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// See comment in `include.rs` for why we disable the prelude.
#![no_implicit_prelude]
#![allow(warnings)]

include!("include.rs");

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum Foo {
    A,
}

util_assert_impl_all!(Foo: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum Bar {
    A = 0,
}

util_assert_impl_all!(Bar: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum TwoVariantsHasExplicitZero {
    A = 1,
    B = 0,
}

util_assert_impl_all!(TwoVariantsHasExplicitZero: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i8)]
enum ImplicitNonFirstVariantIsZero {
    A = -1,
    B,
}

util_assert_impl_all!(ImplicitNonFirstVariantIsZero: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u64)]
enum LargeDiscriminant {
    A = 0xFFFF_FFFF_FFFF_FFFF,
    B = 0x0000_0000_0000_0000,
}

util_assert_impl_all!(LargeDiscriminant: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum FirstVariantIsZeroable {
    A(u32),
    B { foo: u32 },
}

util_assert_impl_all!(FirstVariantIsZeroable: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum FirstVariantIsZeroableSecondIsNot {
    A(bool),
    B(::core::num::NonZeroU8),
}

util_assert_impl_all!(FirstVariantIsZeroableSecondIsNot: imp::FromZeros);

// MSRV does not support data-carrying enum variants with explicit discriminants
#[cfg(not(__ZEROCOPY_TOOLCHAIN = "msrv"))]
mod msrv_only {
    use super::*;

    #[derive(imp::FromZeros)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(u8)]
    enum ImplicitFirstVariantIsZeroable {
        A(bool),
        B(::core::num::NonZeroU8) = 1,
    }

    util_assert_impl_all!(ImplicitFirstVariantIsZeroable: imp::FromZeros);

    #[derive(imp::FromZeros)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(i8)]
    enum ImplicitNonFirstVariantIsZeroable {
        A(::core::num::NonZeroU8) = 1,
        B = -1,
        C(bool),
    }

    util_assert_impl_all!(ImplicitNonFirstVariantIsZeroable: imp::FromZeros);
}
