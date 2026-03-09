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

// A struct is `IntoBytes` if:
// - all fields are `IntoBytes`
// - `repr(C)` or `repr(transparent)` and
//   - no padding (size of struct equals sum of size of field types)
// - `repr(packed)`

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct CZst;

util_assert_impl_all!(CZst: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct C {
    a: u8,
    b: u8,
    c: util::AU16,
}

util_assert_impl_all!(C: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct SyntacticUnsized {
    a: u8,
    b: u8,
    c: [util::AU16],
}

util_assert_impl_all!(C: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct Transparent {
    a: u8,
    b: (),
}

util_assert_impl_all!(Transparent: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct TransparentGeneric<T: ?imp::Sized> {
    a: (),
    b: T,
}

util_assert_impl_all!(TransparentGeneric<u64>: imp::IntoBytes);
util_assert_impl_all!(TransparentGeneric<[u64]>: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
struct CZstPacked;

util_assert_impl_all!(CZstPacked: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
struct CPacked {
    a: u8,
    // NOTE: The `u16` type is not guaranteed to have alignment 2, although it
    // does on many platforms. However, to fix this would require a custom type
    // with a `#[repr(align(2))]` attribute, and `#[repr(packed)]` types are not
    // allowed to transitively contain `#[repr(align(...))]` types. Thus, we
    // have no choice but to use `u16` here. Luckily, these tests run in CI on
    // platforms on which `u16` has alignment 2, so this isn't that big of a
    // deal.
    b: u16,
}

util_assert_impl_all!(CPacked: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed(2))]
// The same caveats as for CPacked apply - we're assuming u64 is at least
// 4-byte aligned by default. Without packed(2), this should fail, as there
// would be padding between a/b assuming u64 is 4+ byte aligned.
struct CPacked2 {
    a: u16,
    b: u64,
}

util_assert_impl_all!(CPacked2: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
struct CPackedGeneric<T, U: ?imp::Sized> {
    t: T,
    // Unsized types stored in `repr(packed)` structs must not be dropped
    // because dropping them in-place might be unsound depending on the
    // alignment of the outer struct. Sized types can be dropped by first being
    // moved to an aligned stack variable, but this isn't possible with unsized
    // types.
    u: imp::ManuallyDrop<U>,
}

util_assert_impl_all!(CPackedGeneric<u8, util::AU16>: imp::IntoBytes);
util_assert_impl_all!(CPackedGeneric<u8, [util::AU16]>: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed)]
struct PackedGeneric<T, U: ?imp::Sized> {
    t: T,
    // Unsized types stored in `repr(packed)` structs must not be dropped
    // because dropping them in-place might be unsound depending on the
    // alignment of the outer struct. Sized types can be dropped by first being
    // moved to an aligned stack variable, but this isn't possible with unsized
    // types.
    u: imp::ManuallyDrop<U>,
}

util_assert_impl_all!(PackedGeneric<u8, util::AU16>: imp::IntoBytes);
util_assert_impl_all!(PackedGeneric<u8, [util::AU16]>: imp::IntoBytes);

// This test is non-portable, but works so long as Rust happens to lay this
// struct out with no padding.
#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
struct Unpacked {
    a: u8,
    b: u8,
}

util_assert_impl_all!(Unpacked: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct ReprCGenericOneField<T: ?imp::Sized> {
    t: T,
}

// Even though `ReprCGenericOneField` has generic type arguments, since it only
// has one field, we don't require that its field types implement `Unaligned`.
util_assert_impl_all!(ReprCGenericOneField<util::AU16>: imp::IntoBytes);
util_assert_impl_all!(ReprCGenericOneField<[util::AU16]>: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct ReprCGenericMultipleFields<T, U: ?imp::Sized> {
    t: T,
    u: U,
}

// Since `ReprCGenericMultipleFields` is generic and has more than one field,
// all field types must implement `Unaligned`.
util_assert_impl_all!(ReprCGenericMultipleFields<u8, [u8; 2]>: imp::IntoBytes);
util_assert_impl_all!(ReprCGenericMultipleFields<u8, [[u8; 2]]>: imp::IntoBytes);
util_assert_not_impl_any!(ReprCGenericMultipleFields<u8, util::AU16>: imp::IntoBytes);
util_assert_not_impl_any!(ReprCGenericMultipleFields<u8, [util::AU16]>: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct Unsized {
    a: [u8],
}

util_assert_impl_all!(Unsized: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, align(2))]
struct UnsizedAligned {
    a: [[u8; 2]],
}

util_assert_impl_all!(UnsizedAligned: imp::IntoBytes);

// Deriving `IntoBytes` should work if the struct has bounded parameters.

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct WithParams<'a: 'b, 'b: 'a, T: 'a + 'b + imp::IntoBytes, const N: usize>(
    [T; N],
    imp::PhantomData<&'a &'b ()>,
)
where
    'a: 'b,
    'b: 'a,
    T: 'a + 'b + imp::IntoBytes;

util_assert_impl_all!(WithParams<'static, 'static, u8, 42>: imp::IntoBytes);

// Test for the failure reported in #1182.

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
pub struct IndexEntryFlags(u8);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
pub struct IndexEntry<const SIZE_BLOCK_ID: usize> {
    block_number: imp::native_endian::U64,
    flags: IndexEntryFlags,
    block_id: [u8; SIZE_BLOCK_ID],
}

util_assert_impl_all!(IndexEntry<0>: imp::IntoBytes);
util_assert_impl_all!(IndexEntry<1>: imp::IntoBytes);
