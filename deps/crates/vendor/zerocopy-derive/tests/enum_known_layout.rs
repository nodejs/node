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

include!("include.rs");

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
enum Foo {
    A,
}

util_assert_impl_all!(Foo: imp::KnownLayout);

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
enum Bar {
    A = 0,
}

util_assert_impl_all!(Bar: imp::KnownLayout);

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
enum Baz {
    A = 1,
    B = 0,
}

util_assert_impl_all!(Baz: imp::KnownLayout);

// Deriving `KnownLayout` should work if the enum has bounded parameters.

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::KnownLayout, const N: usize>
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::KnownLayout,
{
    Variant([T; N], imp::PhantomData<&'a &'b ()>),
}

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::KnownLayout);
