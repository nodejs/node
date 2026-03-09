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

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Zst;

util_assert_impl_all!(Zst: imp::Immutable);

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
struct One {
    a: bool,
}

util_assert_impl_all!(One: imp::Immutable);

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Two {
    a: bool,
    b: Zst,
}

util_assert_impl_all!(Two: imp::Immutable);

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Three {
    a: [u8],
}

util_assert_impl_all!(Three: imp::Immutable);

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Four<'a> {
    field: &'a imp::UnsafeCell<u8>,
}

util_assert_impl_all!(Four<'static>: imp::Immutable);

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
struct TypeParams<'a, T, U, I: imp::Iterator> {
    a: I::Item,
    b: u8,
    c: imp::PhantomData<&'a [::core::primitive::u8]>,
    d: imp::PhantomData<&'static ::core::primitive::str>,
    e: imp::PhantomData<imp::String>,
    f: imp::PhantomData<U>,
    g: T,
}

util_assert_impl_all!(TypeParams<'static, (), (), imp::IntoIter<()>>: imp::Immutable);
util_assert_impl_all!(TypeParams<'static, util::AU16, util::AU16, imp::IntoIter<()>>: imp::Immutable);
util_assert_impl_all!(TypeParams<'static, util::AU16, imp::UnsafeCell<u8>, imp::IntoIter<()>>: imp::Immutable);
util_assert_not_impl_any!(TypeParams<'static, imp::UnsafeCell<()>, (), imp::IntoIter<()>>: imp::Immutable);
util_assert_not_impl_any!(TypeParams<'static, [imp::UnsafeCell<u8>; 0], (), imp::IntoIter<()>>: imp::Immutable);
util_assert_not_impl_any!(TypeParams<'static, (), (), imp::IntoIter<imp::UnsafeCell<()>>>: imp::Immutable);

trait Trait {
    type Assoc;
}

impl<T> Trait for imp::UnsafeCell<T> {
    type Assoc = T;
}

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
struct WithAssocType<T: Trait> {
    field: <T as Trait>::Assoc,
}

util_assert_impl_all!(WithAssocType<imp::UnsafeCell<u8>>: imp::Immutable);

// Deriving `Immutable` should work if the struct has bounded parameters.

#[derive(imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::Immutable, const N: usize>(
    [T; N],
    imp::PhantomData<&'a &'b ()>,
    imp::PhantomData<imp::UnsafeCell<()>>,
    &'a imp::UnsafeCell<()>,
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::Immutable;

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::Immutable);
