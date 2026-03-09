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

// A union is `imp::FromBytes` if:
// - all fields are `imp::FromBytes`

#[derive(Clone, Copy, imp::Immutable, imp::FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
union Zst {
    a: (),
}

util_assert_impl_all!(Zst: imp::FromBytes);
test_trivial_is_bit_valid!(Zst => test_zst_trivial_is_bit_valid);

#[derive(imp::Immutable, imp::FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
union One {
    a: u8,
}

util_assert_impl_all!(One: imp::FromBytes);
test_trivial_is_bit_valid!(One => test_one_trivial_is_bit_valid);

#[derive(imp::Immutable, imp::FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
union Two {
    a: u8,
    b: Zst,
}

util_assert_impl_all!(Two: imp::FromBytes);
test_trivial_is_bit_valid!(Two => test_two_trivial_is_bit_valid);

#[derive(imp::Immutable, imp::FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
union TypeParams<'a, T: imp::Copy, I: imp::Iterator>
where
    I::Item: imp::Copy,
{
    a: T,
    c: I::Item,
    d: u8,
    e: imp::PhantomData<&'a [u8]>,
    f: imp::PhantomData<&'static str>,
    g: imp::PhantomData<imp::String>,
}

util_assert_impl_all!(TypeParams<'static, (), imp::IntoIter<()>>: imp::FromBytes);
test_trivial_is_bit_valid!(TypeParams<'static, (), imp::IntoIter<()>> => test_type_params_trivial_is_bit_valid);

// Deriving `imp::FromBytes` should work if the union has bounded parameters.

#[derive(imp::Immutable, imp::FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::FromBytes, const N: usize>
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::Copy + imp::FromBytes,
{
    a: [T; N],
    b: imp::PhantomData<&'a &'b ()>,
}

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::FromBytes);
test_trivial_is_bit_valid!(WithParams<'static, 'static, u8, 42> => test_with_params_trivial_is_bit_valid);

#[derive(imp::FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union UnsafeCellUnion {
    a: imp::ManuallyDrop<imp::UnsafeCell<u8>>,
}

util_assert_impl_all!(UnsafeCellUnion: imp::FromBytes);
test_trivial_is_bit_valid!(UnsafeCellUnion => test_unsafe_cell_union_trivial_is_bit_valid);
