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

// A struct is `Unaligned` if:
// - `repr(align)` is no more than 1 and either
//   - `repr(C)` or `repr(transparent)` and
//     - all fields Unaligned
//   - `repr(packed)`

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct Foo {
    a: u8,
}

util_assert_impl_all!(Foo: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct Bar {
    a: u8,
}

util_assert_impl_all!(Bar: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed)]
struct Baz {
    // NOTE: The `u16` type is not guaranteed to have alignment 2, although it
    // does on many platforms. However, to fix this would require a custom type
    // with a `#[repr(align(2))]` attribute, and `#[repr(packed)]` types are not
    // allowed to transitively contain `#[repr(align(...))]` types. Thus, we
    // have no choice but to use `u16` here. Luckily, these tests run in CI on
    // platforms on which `u16` has alignment 2, so this isn't that big of a
    // deal.
    a: u16,
}

util_assert_impl_all!(Baz: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, align(1))]
struct FooAlign {
    a: u8,
}

util_assert_impl_all!(FooAlign: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct Unsized {
    a: [u8],
}

util_assert_impl_all!(Unsized: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct TypeParams<'a, T: ?imp::Sized, I: imp::Iterator> {
    a: I::Item,
    b: u8,
    c: imp::PhantomData<&'a [::core::primitive::u8]>,
    d: imp::PhantomData<&'static ::core::primitive::str>,
    e: imp::PhantomData<imp::String>,
    f: T,
}

util_assert_impl_all!(TypeParams<'static, (), imp::IntoIter<()>>: imp::Unaligned);
util_assert_impl_all!(TypeParams<'static, ::core::primitive::u8, imp::IntoIter<()>>: imp::Unaligned);
util_assert_impl_all!(TypeParams<'static, [::core::primitive::u8], imp::IntoIter<()>>: imp::Unaligned);

// Deriving `Unaligned` should work if the struct has bounded parameters.

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::Unaligned, const N: usize>(
    [T; N],
    imp::PhantomData<&'a &'b ()>,
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::Unaligned;

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::Unaligned);
