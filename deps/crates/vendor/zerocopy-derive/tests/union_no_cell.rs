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

#[derive(Clone, Copy, imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
union Zst {
    a: (),
}

util_assert_impl_all!(Zst: imp::Immutable);

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
union One {
    a: bool,
}

util_assert_impl_all!(One: imp::Immutable);

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
union Two {
    a: bool,
    b: Zst,
}

util_assert_impl_all!(Two: imp::Immutable);

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
union TypeParams<'a, T: imp::Copy, I: imp::Iterator>
where
    I::Item: imp::Copy,
{
    a: T,
    c: I::Item,
    d: u8,
    e: imp::PhantomData<&'a [::core::primitive::u8]>,
    f: imp::PhantomData<&'static ::core::primitive::str>,
    g: imp::PhantomData<imp::String>,
}

util_assert_impl_all!(TypeParams<'static, (), imp::IntoIter<()>>: imp::Immutable);

// Deriving `imp::Immutable` should work if the union has bounded parameters.

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::Immutable, const N: usize>
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::Copy + imp::Immutable,
{
    a: [T; N],
    b: imp::PhantomData<&'a &'b ()>,
    c: imp::PhantomData<imp::UnsafeCell<()>>,
    d: &'a imp::UnsafeCell<()>,
}

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::Immutable);
