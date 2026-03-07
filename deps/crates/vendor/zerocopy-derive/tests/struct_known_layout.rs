// Copyright 2023 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// See comment in `include.rs` for why we disable the prelude.
#![no_implicit_prelude]
#![allow(warnings)]

extern crate rustversion;

include!("include.rs");

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Zst;

util_assert_impl_all!(Zst: imp::KnownLayout);

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
struct One {
    a: bool,
}

util_assert_impl_all!(One: imp::KnownLayout);

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Two {
    a: bool,
    b: Zst,
}

util_assert_impl_all!(Two: imp::KnownLayout);

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
struct TypeParams<'a, T, I: imp::Iterator> {
    a: I::Item,
    b: u8,
    c: imp::PhantomData<&'a [::core::primitive::u8]>,
    d: imp::PhantomData<&'static ::core::primitive::str>,
    e: imp::PhantomData<imp::String>,
    f: T,
}

util_assert_impl_all!(TypeParams<'static, (), imp::IntoIter<()>>: imp::KnownLayout);
util_assert_impl_all!(TypeParams<'static, util::AU16, imp::IntoIter<()>>: imp::KnownLayout);

// Deriving `KnownLayout` should work if the struct has bounded parameters.
//
// N.B. We limit this test to rustc >= 1.62, since earlier versions of rustc ICE
// when `KnownLayout` is derived on a `repr(C)` struct whose trailing field
// contains non-static lifetimes.
#[rustversion::since(1.62)]
const _: () = {
    #[derive(imp::KnownLayout)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(C)]
    struct WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::KnownLayout, const N: usize>(
        [T; N],
        imp::PhantomData<&'a &'b ()>,
    )
    where
        'a: 'b,
        'b: 'a,
        T: 'a + 'b + imp::KnownLayout;

    util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::KnownLayout);
};

const _: () = {
    // Similar to the previous test, except that the trailing field contains
    // only static lifetimes. This is exercisable on all supported toolchains.

    #[derive(imp::KnownLayout)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(C)]
    struct WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::KnownLayout, const N: usize>(
        &'a &'b [T; N],
        imp::PhantomData<&'static ()>,
    )
    where
        'a: 'b,
        'b: 'a,
        T: 'a + 'b + imp::KnownLayout;

    util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::KnownLayout);
};

// Deriving `KnownLayout` should work if the struct references `Self`. See
// #2116.

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct WithSelfReference {
    leading: [u8; Self::N],
    trailing: [[u8; Self::N]],
}

impl WithSelfReference {
    const N: usize = 42;
}

util_assert_impl_all!(WithSelfReference: imp::KnownLayout);

// Deriving `KnownLayout` should work with generic `repr(packed)` types. See
// #2302.

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
struct Packet<P> {
    payload: P,
}

util_assert_impl_all!(Packet<imp::u8>: imp::KnownLayout);

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct RawIdentifier {
    r#type: u8,
}

util_assert_impl_all!(RawIdentifier: imp::KnownLayout);
