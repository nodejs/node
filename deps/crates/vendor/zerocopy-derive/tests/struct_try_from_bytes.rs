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
// - all fields are `imp::TryFromBytes`

#[test]
fn zst() {
    crate::util::test_is_bit_valid::<(), _>((), true);
}

#[derive(imp::TryFromBytes, imp::Immutable, imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct One {
    a: u8,
}

util_assert_impl_all!(One: imp::TryFromBytes);

#[test]
fn one() {
    crate::util::test_is_bit_valid::<One, _>(One { a: 42 }, true);
    crate::util::test_is_bit_valid::<One, _>(One { a: 43 }, true);
}

#[derive(imp::TryFromBytes, imp::Immutable, imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct Two {
    a: bool,
    b: (),
}

util_assert_impl_all!(Two: imp::TryFromBytes);

#[test]
fn two() {
    crate::util::test_is_bit_valid::<Two, _>(Two { a: false, b: () }, true);
    crate::util::test_is_bit_valid::<Two, _>(Two { a: true, b: () }, true);
    crate::util::test_is_bit_valid::<Two, _>([2u8], false);
}

#[derive(imp::KnownLayout, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct Unsized {
    a: [u8],
}

util_assert_impl_all!(Unsized: imp::TryFromBytes);

#[test]
fn un_sized() {
    // FIXME(#5): Use `try_transmute` in this test once it's available.
    let mut buf = [16u8, 12, 42];
    let candidate = ::zerocopy_renamed::Ptr::from_mut(&mut buf[..]);
    // SAFETY: `&Unsized` consists entirely of initialized bytes.
    let candidate = unsafe { candidate.assume_initialized() };

    let mut candidate = {
        use imp::pointer::{cast::CastUnsized, BecauseExclusive};
        candidate.cast::<_, CastUnsized, (_, BecauseExclusive)>()
    };

    // SAFETY: `candidate`'s referent is as-initialized as `Two`.
    let mut candidate = unsafe { candidate.assume_initialized() };
    let is_bit_valid = <Unsized as imp::TryFromBytes>::is_bit_valid(candidate.reborrow_shared());
    imp::assert!(is_bit_valid);
}

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct TypeParams<'a, T: ?imp::Sized, I: imp::Iterator> {
    a: I::Item,
    b: u8,
    c: imp::PhantomData<&'a [u8]>,
    d: imp::PhantomData<&'static str>,
    e: imp::PhantomData<imp::String>,
    f: T,
}

util_assert_impl_all!(TypeParams<'static, (), imp::IntoIter<()>>: imp::TryFromBytes);
util_assert_impl_all!(TypeParams<'static, util::AU16, imp::IntoIter<()>>: imp::TryFromBytes);
util_assert_impl_all!(TypeParams<'static, [util::AU16], imp::IntoIter<()>>: imp::TryFromBytes);

// Deriving `imp::TryFromBytes` should work if the struct has bounded
// parameters.

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::TryFromBytes, const N: usize>(
    imp::PhantomData<&'a &'b ()>,
    [T],
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::TryFromBytes;

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::TryFromBytes);

#[derive(imp::FromBytes, imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct MaybeFromBytes<T>(T);

#[test]
fn test_maybe_from_bytes() {
    // When deriving `FromBytes` on a type with no generic parameters, we emit a
    // trivial `is_bit_valid` impl that always returns true. This test confirms
    // that we *don't* spuriously do that when generic parameters are present.

    crate::util::test_is_bit_valid::<MaybeFromBytes<bool>, _>(MaybeFromBytes(false), true);
    crate::util::test_is_bit_valid::<MaybeFromBytes<bool>, _>(MaybeFromBytes(true), true);
    crate::util::test_is_bit_valid::<MaybeFromBytes<bool>, _>([2u8], false);
}

#[derive(Debug, PartialEq, Eq, imp::TryFromBytes, imp::Immutable, imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
struct CPacked {
    a: u8,
    // NOTE: The `u32` type is not guaranteed to have alignment 4, although it
    // does on many platforms. However, to fix this would require a custom type
    // with a `#[repr(align(4))]` attribute, and `#[repr(packed)]` types are not
    // allowed to transitively contain `#[repr(align(...))]` types. Thus, we
    // have no choice but to use `u32` here. Luckily, these tests run in CI on
    // platforms on which `u32` has alignment 4, so this isn't that big of a
    // deal.
    b: u32,
}

#[test]
fn c_packed() {
    let candidate = &[42u8, 0xFF, 0xFF, 0xFF, 0xFF];
    let converted = <CPacked as imp::TryFromBytes>::try_ref_from_bytes(candidate);
    imp::assert_eq!(converted, imp::Ok(&CPacked { a: 42, b: u32::MAX }));
}

#[derive(imp::TryFromBytes, imp::KnownLayout, imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
struct CPackedUnsized {
    a: u8,
    // NOTE: The `u32` type is not guaranteed to have alignment 4, although it
    // does on many platforms. However, to fix this would require a custom type
    // with a `#[repr(align(4))]` attribute, and `#[repr(packed)]` types are not
    // allowed to transitively contain `#[repr(align(...))]` types. Thus, we
    // have no choice but to use `u32` here. Luckily, these tests run in CI on
    // platforms on which `u32` has alignment 4, so this isn't that big of a
    // deal.
    b: [u32],
}

#[test]
fn c_packed_unsized() {
    let candidate = &[42u8, 0xFF, 0xFF, 0xFF, 0xFF];
    let converted = <CPackedUnsized as imp::TryFromBytes>::try_ref_from_bytes(candidate);
    imp::assert!(converted.is_ok());
}

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed)]
struct PackedUnsized {
    a: u8,
    // NOTE: The `u32` type is not guaranteed to have alignment 4, although it
    // does on many platforms. However, to fix this would require a custom type
    // with a `#[repr(align(4))]` attribute, and `#[repr(packed)]` types are not
    // allowed to transitively contain `#[repr(align(...))]` types. Thus, we
    // have no choice but to use `u32` here. Luckily, these tests run in CI on
    // platforms on which `u32` has alignment 4, so this isn't that big of a
    // deal.
    b: [u32],
}

#[test]
fn packed_unsized() {
    let candidate = &[42u8, 0xFF, 0xFF, 0xFF, 0xFF];
    let converted = <CPackedUnsized as imp::TryFromBytes>::try_ref_from_bytes(candidate);
    imp::assert!(converted.is_ok());

    let candidate = &[42u8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF];
    let converted = <CPackedUnsized as imp::TryFromBytes>::try_ref_from_bytes(candidate);
    imp::assert!(converted.is_err());

    let candidate = &[42u8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF];
    let converted = <CPackedUnsized as imp::TryFromBytes>::try_ref_from_bytes(candidate);
    imp::assert!(converted.is_ok());
}

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
struct A;

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
struct B {
    a: A,
}

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct RawIdent {
    r#type: u8,
}

util_assert_impl_all!(RawIdent: imp::TryFromBytes);
