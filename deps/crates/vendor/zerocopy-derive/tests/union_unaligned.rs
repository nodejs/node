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

// A union is `Unaligned` if:
// - `repr(align)` is no more than 1 and either
//   - `repr(C)` or `repr(transparent)` and
//     - all fields `Unaligned`
//   - `repr(packed)`

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union Foo {
    a: imp::u8,
}

util_assert_impl_all!(Foo: imp::Unaligned);

// Transparent unions are unstable; see issue #60405
// <https://github.com/rust-lang/rust/issues/60405> for more information.

// #[derive(Unaligned)]
// #[repr(transparent)]
// union Bar {
//     a: u8,
// }

// is_unaligned!(Bar);
#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed)]
union Baz {
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
union FooAlign {
    a: imp::u8,
}

util_assert_impl_all!(FooAlign: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union TypeParams<'a, T: imp::Copy, I: imp::Iterator>
where
    I::Item: imp::Copy,
{
    a: T,
    c: I::Item,
    d: u8,
    e: imp::PhantomData<&'a [imp::u8]>,
    f: imp::PhantomData<&'static imp::str>,
    g: imp::PhantomData<imp::String>,
}

util_assert_impl_all!(TypeParams<'static, (), imp::IntoIter<()>>: imp::Unaligned);
