// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#[macro_use]
extern crate zerocopy_renamed;

#[path = "../include.rs"]
mod util;

use zerocopy_renamed::{IntoBytes, KnownLayout};

use self::util::util::AU16;

fn main() {}

//
// KnownLayout errors
//

struct NotKnownLayout;

struct NotKnownLayoutDst([u8]);

// -| `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
// -|          N |        N |              N |        N |      KL00 |
#[derive(KnownLayout)]
//~[msrv, stable, nightly]^ ERROR: the size for values of type `[u8]` cannot be known at compilation time
#[zerocopy(crate = "zerocopy_renamed")]
struct KL00(u8, NotKnownLayoutDst);

// -| `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
// -|          N |        N |              Y |        N |      KL02 |
#[derive(KnownLayout)]
//~[msrv, stable, nightly]^ ERROR: the size for values of type `[u8]` cannot be known at compilation time
#[zerocopy(crate = "zerocopy_renamed")]
struct KL02(u8, [u8]);

// -| `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
// -|          Y |        N |              N |        N |      KL08 |
#[derive(KnownLayout)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotKnownLayoutDst: zerocopy_renamed::KnownLayout` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct KL08(u8, NotKnownLayoutDst);

// -| `repr(C)`? | generic? | `KnownLayout`? | `Sized`? | Type Name |
// -|          Y |        N |              N |        Y |      KL09 |
#[derive(KnownLayout)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotKnownLayout: zerocopy_renamed::KnownLayout` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct KL09(NotKnownLayout, NotKnownLayout);

//
// Immutable errors
//

#[derive(Immutable)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `UnsafeCell<()>: zerocopy_renamed::Immutable` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
struct Immutable1 {
    a: core::cell::UnsafeCell<()>,
}

#[derive(Immutable)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `UnsafeCell<u8>: zerocopy_renamed::Immutable` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
struct Immutable2 {
    a: [core::cell::UnsafeCell<u8>; 0],
}

//
// TryFromBytes errors
//

#[derive(TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed)]
struct TryFromBytesPacked {
    //~[stable, nightly]^ ERROR: packed type cannot transitively contain a `#[repr(align)]` type
    foo: AU16,
}

#[derive(TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed(1))]
struct TryFromBytesPackedN {
    //~[stable, nightly]^ ERROR: packed type cannot transitively contain a `#[repr(align)]` type
    foo: AU16,
}

#[derive(TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
struct TryFromBytesCPacked {
    //~[stable, nightly]^ ERROR: packed type cannot transitively contain a `#[repr(align)]` type
    foo: AU16,
}

#[derive(TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed(1))]
struct TryFromBytesCPackedN {
    //~[stable, nightly]^ ERROR: packed type cannot transitively contain a `#[repr(align)]` type
    foo: AU16,
}

//
// IntoBytes errors
//

// Since `IntoBytes1` has at least one generic parameter, an `IntoBytes` impl is
// emitted in which each field type is given an `Unaligned` bound. Since `foo`'s
// type doesn't implement `Unaligned`, this should fail.
#[derive(IntoBytes)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `AU16: zerocopy_renamed::Unaligned` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct IntoBytes1<T> {
    foo: AU16,
    bar: T,
}

#[derive(IntoBytes)]
//~[stable, nightly]^ ERROR: `IntoBytes2` has 1 total byte(s) of padding
//~[msrv]^^ ERROR: the trait bound `(): PaddingFree<IntoBytes2, 1_usize>` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct IntoBytes2 {
    foo: u8,
    bar: AU16,
}

#[derive(IntoBytes)]
//~[stable, nightly]^ ERROR: `IntoBytes3` has 1 total byte(s) of padding
//~[msrv]^^ ERROR: the trait bound `(): PaddingFree<IntoBytes3, 1_usize>` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed(2))]
struct IntoBytes3 {
    foo: u8,
    // We'd prefer to use AU64 here, but you can't use aligned types in
    // packed structs.
    bar: u64,
}

type SliceU8 = [u8];

// Padding between `u8` and `SliceU8`. `SliceU8` doesn't syntactically look like
// a slice, so this case is handled by our `Sized` support.
//
// NOTE(#1708): This exists to ensure that our error messages are good when a
// field is unsized.
#[derive(IntoBytes)]
//~[stable, nightly]^ ERROR: the size for values of type `[u8]` cannot be known at compilation time
//~[msrv]^^ ERROR: the size for values of type `[u8]` cannot be known at compilation time
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct IntoBytes4 {
    a: u8,
    b: SliceU8,
    //~[stable, nightly]^ ERROR: `[u8]` is unsized
    //~[msrv]^^ ERROR: the size for values of type `[u8]` cannot be known at compilation time
}

// Padding between `u8` and `[u16]`. `[u16]` is syntactically identifiable as a
// slice, so this case is handled by our `repr(C)` slice DST support.
#[derive(IntoBytes)]
//~[msrv]^ ERROR: the trait bound `(): DynamicPaddingFree<IntoBytes5, true>` is not satisfied
//~[stable, nightly]^^ ERROR: `IntoBytes5` has one or more padding bytes
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct IntoBytes5 {
    a: u8,
    b: [u16],
}

// Trailing padding after `[u8]`. `[u8]` is syntactically identifiable as a
// slice, so this case is handled by our `repr(C)` slice DST support.
#[derive(IntoBytes)]
//~[msrv]^ ERROR: the trait bound `(): DynamicPaddingFree<IntoBytes6, true>` is not satisfied
//~[stable, nightly]^^ ERROR: `IntoBytes6` has one or more padding bytes
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct IntoBytes6 {
    a: u16,
    b: [u8],
}

// Padding between `u8` and `u16` and also trailing padding after `[u8]`. `[u8]`
// is syntactically identifiable as a slice, so this case is handled by our
// `repr(C)` slice DST support.
#[derive(IntoBytes)]
//~[msrv]^ ERROR: the trait bound `(): DynamicPaddingFree<IntoBytes7, true>` is not satisfied
//~[stable, nightly]^^ ERROR: `IntoBytes7` has one or more padding bytes
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct IntoBytes7 {
    a: u8,
    b: u16,
    c: [u8],
}

#[derive(IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, C)] // zerocopy-derive conservatively treats these as conflicting reprs
              //~[msrv, stable, nightly]^ ERROR: this conflicts with another representation hint
struct IntoBytes8 {
    a: u8,
}

#[derive(IntoBytes)]
//~[msrv, stable, nightly]^ ERROR: must have a non-align #[repr(...)] attribute in order to guarantee this type's memory layout
#[zerocopy(crate = "zerocopy_renamed")]
struct IntoBytes9<T> {
    t: T,
}

#[derive(IntoBytes)]
//~[msrv, stable, nightly]^ ERROR: must have a non-align #[repr(...)] attribute in order to guarantee this type's memory layout
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed(2))]
struct IntoBytes10<T> {
    t: T,
}

// `repr(C, packed(2))` is not equivalent to `repr(C, packed)`.
#[derive(IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed(2))]
struct IntoBytes11<T> {
    t0: T,
    // Add a second field to avoid triggering the "repr(C) struct with one
    // field" special case.
    t1: T,
}

fn is_into_bytes_11<T: IntoBytes>() {
    if false {
        is_into_bytes_11::<IntoBytes11<AU16>>();
        //~[stable, nightly]^ ERROR: the trait bound `AU16: zerocopy_renamed::Unaligned` is not satisfied
    }
}

// `repr(C, align(2))` is not sufficient to guarantee the layout of this type.
#[derive(IntoBytes)]
//~[msrv, stable, nightly]^ ERROR: must have a non-align #[repr(...)] attribute in order to guarantee this type's memory layout
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, align(2))]
struct IntoBytes12<T> {
    t: T,
}

// NOTE(#3063): This exists to ensure that our analysis for structs with
// syntactic DSTs accounts for `align(N)`.
#[derive(IntoBytes)]
//~[msrv]^ ERROR: the trait bound `(): DynamicPaddingFree<IntoBytes13, true>` is not satisfied
//~[stable, nightly]^^ ERROR: `IntoBytes13` has one or more padding bytes
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, align(2))]
struct IntoBytes13([u8]);

//
// Unaligned errors
//

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, align(2))]
//~[msrv, stable, nightly]^ ERROR: cannot derive `Unaligned` on type with alignment greater than 1
struct Unaligned1;

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent, align(2))]
//~[msrv, stable, nightly]^ ERROR: this conflicts with another representation hint
//~[msrv, stable, nightly]^^ ERROR: transparent struct cannot have other repr hints
struct Unaligned2 {
    foo: u8,
}

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed, align(2))]
//~[msrv, stable, nightly]^ ERROR: this conflicts with another representation hint
struct Unaligned3;
//~[stable, nightly]^ ERROR: type has conflicting packed and align representation hints

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(align(1), align(2))]
//~[msrv, stable, nightly]^ ERROR: this conflicts with another representation hint
struct Unaligned4;

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(align(2), align(4))]
//~[msrv, stable, nightly]^ ERROR: this conflicts with another representation hint
struct Unaligned5;

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)], #[repr(transparent)], or #[repr(packed)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
struct Unaligned6;

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)], #[repr(transparent)], or #[repr(packed)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(packed(2))]
struct Unaligned7;

// Test the error message emitted when conflicting reprs appear on different
// lines. On the nightly compiler, this emits a "joint span" that spans both
// problematic repr token trees and everything in between.
#[derive(Copy, Clone)]
#[repr(packed(2), C)]
//~[nightly]^ ERROR: this conflicts with another representation hint
#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed(2))]
//~[msrv, stable]^ ERROR: this conflicts with another representation hint
struct WeirdReprSpan;

#[derive(SplitAt)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `SplitAtNotKnownLayout: zerocopy_renamed::KnownLayout` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct SplitAtNotKnownLayout([u8]);

#[derive(SplitAt, KnownLayout)]
//~[msrv]^ ERROR: type mismatch resolving `<u8 as zerocopy_renamed::KnownLayout>::PointerMetadata == usize`
//~[msrv, stable, nightly]^^ ERROR: the trait bound `u8: SplitAt` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct SplitAtSized(u8);
