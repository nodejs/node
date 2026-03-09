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

// A struct is `FromZeros` if:
// - all fields are `FromZeros`

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Zst;

util_assert_impl_all!(Zst: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
struct One {
    a: bool,
}

util_assert_impl_all!(One: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Two {
    a: bool,
    b: Zst,
}

util_assert_impl_all!(Two: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Unsized {
    a: [u8],
}

util_assert_impl_all!(Unsized: imp::FromZeros);

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
struct TypeParams<'a, T: ?imp::Sized, I: imp::Iterator> {
    a: I::Item,
    b: u8,
    c: imp::PhantomData<&'a [u8]>,
    d: imp::PhantomData<&'static str>,
    e: imp::PhantomData<imp::String>,
    f: T,
}

util_assert_impl_all!(TypeParams<'static, (), imp::IntoIter<()>>: imp::FromZeros);
util_assert_impl_all!(TypeParams<'static, util::AU16, imp::IntoIter<()>>: imp::FromZeros);
util_assert_impl_all!(TypeParams<'static, [util::AU16], imp::IntoIter<()>>: imp::FromZeros);

// Deriving `FromZeros` should work if the struct has bounded parameters.

#[derive(imp::FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::FromZeros, const N: usize>(
    [T; N],
    imp::PhantomData<&'a &'b ()>,
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::FromZeros;

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::FromZeros);
