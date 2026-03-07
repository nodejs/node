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

// A struct is `imp::TryFromBytes` if:
// - any of its fields are `imp::TryFromBytes`

#[derive(imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
union One {
    a: u8,
}

util_assert_impl_all!(One: imp::TryFromBytes);

#[test]
fn one() {
    crate::util::test_is_bit_valid::<One, _>([42u8], true);
    crate::util::test_is_bit_valid::<One, _>([43u8], true);
}

#[derive(imp::Immutable, imp::TryFromBytes, imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union Two {
    a: bool,
    b: bool,
}

util_assert_impl_all!(Two: imp::TryFromBytes);

#[test]
fn two() {
    crate::util::test_is_bit_valid::<Two, _>(Two { a: false }, true);
    crate::util::test_is_bit_valid::<Two, _>(Two { b: true }, true);
    crate::util::test_is_bit_valid::<Two, _>([2u8], false);
}

#[derive(imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union BoolAndZst {
    a: bool,
    b: (),
}

#[test]
fn bool_and_zst() {
    crate::util::test_is_bit_valid::<BoolAndZst, _>([0u8], true);
    crate::util::test_is_bit_valid::<BoolAndZst, _>([1u8], true);
    crate::util::test_is_bit_valid::<BoolAndZst, _>([2u8], true);
}

#[derive(imp::FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union MaybeFromBytes<T: imp::Copy> {
    t: T,
}

#[test]
fn test_maybe_from_bytes() {
    // When deriving `FromBytes` on a type with no generic parameters, we emit a
    // trivial `is_bit_valid` impl that always returns true. This test confirms
    // that we *don't* spuriously do that when generic parameters are present.

    crate::util::test_is_bit_valid::<MaybeFromBytes<bool>, _>([2u8], false);
}

#[derive(imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union TypeParams<'a, T: imp::Copy, I: imp::Iterator>
where
    I::Item: imp::Copy,
{
    a: I::Item,
    b: u8,
    c: imp::PhantomData<&'a [u8]>,
    d: imp::PhantomData<&'static str>,
    e: imp::PhantomData<imp::String>,
    f: T,
}

util_assert_impl_all!(TypeParams<'static, (), imp::IntoIter<()>>: imp::TryFromBytes);
util_assert_impl_all!(TypeParams<'static, util::AU16, imp::IntoIter<()>>: imp::TryFromBytes);
util_assert_impl_all!(TypeParams<'static, [util::AU16; 2], imp::IntoIter<()>>: imp::TryFromBytes);

// Deriving `imp::TryFromBytes` should work if the union has bounded parameters.

#[derive(imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::TryFromBytes, const N: usize>
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::TryFromBytes + imp::Copy,
{
    a: imp::PhantomData<&'a &'b ()>,
    b: T,
}

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::TryFromBytes);

#[derive(Clone, Copy, imp::TryFromBytes, imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
struct A;

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
union B {
    a: A,
}

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union UnsafeCellUnion {
    a: imp::ManuallyDrop<imp::UnsafeCell<bool>>,
}

util_assert_impl_all!(UnsafeCellUnion: imp::TryFromBytes);

#[test]
fn unsafe_cell_union() {
    crate::util::test_is_bit_valid::<UnsafeCellUnion, _>([0u8], true);
    crate::util::test_is_bit_valid::<UnsafeCellUnion, _>([1u8], true);
    crate::util::test_is_bit_valid::<UnsafeCellUnion, _>([2u8], false);
}
