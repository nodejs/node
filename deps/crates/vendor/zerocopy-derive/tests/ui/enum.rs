// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#[macro_use]
extern crate zerocopy_renamed;

fn main() {}

//
// Generic errors
//

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr("foo")]
//~[msrv, stable, nightly]^ ERROR: unrecognized representation hint
//~[msrv, stable, nightly]^^ ERROR: meta item in `repr` must be an identifier
enum Generic1 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(foo)]
//~[msrv, stable, nightly]^ ERROR: unrecognized representation hint
//~[msrv, stable, nightly]^^ ERROR: unrecognized representation hint
enum Generic2 {
    A,
}

#[derive(FromBytes)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
enum Generic3 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8, u16)]
//~[msrv, stable, nightly]^ ERROR: this conflicts with another representation hint
//~[msrv, stable, nightly]^^ ERROR: conflicting representation hints
enum Generic4 {
    A,
}

#[derive(FromBytes)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
#[zerocopy(crate = "zerocopy_renamed")]
enum Generic5 {
    A,
}

//
// Immutable errors
//

#[derive(Immutable)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `UnsafeCell<()>: Immutable` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
enum Immutable1 {
    A(core::cell::UnsafeCell<()>),
}

#[derive(Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
enum Never {}

#[derive(Immutable)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `UnsafeCell<u8>: Immutable` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
enum Immutable2 {
    Uninhabited(Never, core::cell::UnsafeCell<u8>),
    Inhabited(u8),
}

//
// TryFromBytes errors
//

#[derive(TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
enum TryFromBytes1 {
    A,
}

#[derive(TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
enum TryFromBytes2 {
    A,
    B(u8),
}

struct NotTryFromBytes;

#[derive(TryFromBytes)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotTryFromBytes: TryFromBytes` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum TryFromBytes3 {
    A(NotTryFromBytes),
}

//
// FromZeros errors
//

#[derive(FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
enum FromZeros1 {
    A(u8),
}

#[derive(FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
enum FromZeros2 {
    A,
    B(u8),
}

#[derive(FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
enum FromZeros3 {
    A = 1,
    B,
}

#[derive(FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
//~[msrv, stable, nightly]^ ERROR: FromZeros only supported on enums with a variant that has a discriminant of `0`
#[repr(u8)]
enum FromZeros4 {
    A = 1,
    B = 2,
}

const NEGATIVE_ONE: i8 = -1;

#[derive(FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
//~[msrv, stable, nightly]^ ERROR: FromZeros only supported on enums with a variant that has a discriminant of `0`
#[repr(i8)]
enum FromZeros5 {
    A = NEGATIVE_ONE,
    B,
}

struct NotFromZeros;

#[derive(FromZeros)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotFromZeros: TryFromBytes` is not satisfied
//~[msrv, stable, nightly]^^ ERROR: the trait bound `NotFromZeros: FromZeros` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum FromZeros6 {
    A(NotFromZeros),
}

#[derive(FromZeros)]
#[zerocopy(crate = "zerocopy_renamed")]
//~[msrv, stable, nightly]^ ERROR: FromZeros only supported on enums with a variant that has a discriminant of `0`
#[repr(u8)]
enum FromZeros7 {
    A = 1,
    //~[msrv]^ ERROR: custom discriminant values are not allowed in enums with tuple or struct variants
    B(NotFromZeros),
}

//
// FromBytes errors
//

#[derive(FromBytes)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
#[zerocopy(crate = "zerocopy_renamed")]
enum FromBytes1 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
//~[msrv, stable, nightly]^ ERROR: `FromBytes` only supported on enums with `#[repr(...)]` attributes `u8`, `i8`, `u16`, or `i16`
enum FromBytes2 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(usize)]
//~[msrv, stable, nightly]^ ERROR: `FromBytes` only supported on enums with `#[repr(...)]` attributes `u8`, `i8`, `u16`, or `i16`
enum FromBytes3 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(isize)]
//~[msrv, stable, nightly]^ ERROR: `FromBytes` only supported on enums with `#[repr(...)]` attributes `u8`, `i8`, `u16`, or `i16`
enum FromBytes4 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u32)]
//~[msrv, stable, nightly]^ ERROR: `FromBytes` only supported on enums with `#[repr(...)]` attributes `u8`, `i8`, `u16`, or `i16`
enum FromBytes5 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i32)]
//~[msrv, stable, nightly]^ ERROR: `FromBytes` only supported on enums with `#[repr(...)]` attributes `u8`, `i8`, `u16`, or `i16`
enum FromBytes6 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u64)]
//~[msrv, stable, nightly]^ ERROR: `FromBytes` only supported on enums with `#[repr(...)]` attributes `u8`, `i8`, `u16`, or `i16`
enum FromBytes7 {
    A,
}

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i64)]
//~[msrv, stable, nightly]^ ERROR: `FromBytes` only supported on enums with `#[repr(...)]` attributes `u8`, `i8`, `u16`, or `i16`
enum FromBytes8 {
    A,
}

#[derive(FromBytes)]
//~[msrv, stable, nightly]^ ERROR: the trait bound `bool: FromBytes` is not satisfied
//~[stable, nightly]^^ ERROR: the trait bound `bool: FromBytes` is not satisfied
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum FooU8 {
    Variant0,
    Variant1,
    Variant2,
    Variant3,
    Variant4,
    Variant5,
    Variant6,
    Variant7,
    Variant8,
    Variant9,
    Variant10,
    Variant11,
    Variant12,
    Variant13,
    Variant14,
    Variant15,
    Variant16,
    Variant17,
    Variant18,
    Variant19,
    Variant20,
    Variant21,
    Variant22,
    Variant23,
    Variant24,
    Variant25,
    Variant26,
    Variant27,
    Variant28,
    Variant29,
    Variant30,
    Variant31,
    Variant32,
    Variant33,
    Variant34,
    Variant35,
    Variant36,
    Variant37,
    Variant38,
    Variant39,
    Variant40,
    Variant41,
    Variant42,
    Variant43,
    Variant44,
    Variant45,
    Variant46,
    Variant47,
    Variant48,
    Variant49,
    Variant50,
    Variant51,
    Variant52,
    Variant53,
    Variant54,
    Variant55,
    Variant56,
    Variant57,
    Variant58,
    Variant59,
    Variant60,
    Variant61,
    Variant62,
    Variant63,
    Variant64,
    Variant65,
    Variant66,
    Variant67,
    Variant68,
    Variant69,
    Variant70,
    Variant71,
    Variant72,
    Variant73,
    Variant74,
    Variant75,
    Variant76,
    Variant77,
    Variant78,
    Variant79,
    Variant80,
    Variant81,
    Variant82,
    Variant83,
    Variant84,
    Variant85,
    Variant86,
    Variant87,
    Variant88,
    Variant89,
    Variant90,
    Variant91,
    Variant92,
    Variant93,
    Variant94,
    Variant95,
    Variant96,
    Variant97,
    Variant98,
    Variant99,
    Variant100,
    Variant101,
    Variant102,
    Variant103,
    Variant104,
    Variant105,
    Variant106,
    Variant107,
    Variant108,
    Variant109,
    Variant110,
    Variant111,
    Variant112,
    Variant113,
    Variant114,
    Variant115,
    Variant116,
    Variant117,
    Variant118,
    Variant119,
    Variant120,
    Variant121,
    Variant122,
    Variant123,
    Variant124,
    Variant125,
    Variant126,
    Variant127,
    Variant128,
    Variant129,
    Variant130,
    Variant131,
    Variant132,
    Variant133,
    Variant134,
    Variant135,
    Variant136,
    Variant137,
    Variant138,
    Variant139,
    Variant140,
    Variant141,
    Variant142,
    Variant143,
    Variant144,
    Variant145,
    Variant146,
    Variant147,
    Variant148,
    Variant149,
    Variant150,
    Variant151,
    Variant152,
    Variant153,
    Variant154,
    Variant155,
    Variant156,
    Variant157,
    Variant158,
    Variant159,
    Variant160,
    Variant161,
    Variant162,
    Variant163,
    Variant164,
    Variant165,
    Variant166,
    Variant167,
    Variant168,
    Variant169,
    Variant170,
    Variant171,
    Variant172,
    Variant173,
    Variant174,
    Variant175,
    Variant176,
    Variant177,
    Variant178,
    Variant179,
    Variant180,
    Variant181,
    Variant182,
    Variant183,
    Variant184,
    Variant185,
    Variant186,
    Variant187,
    Variant188,
    Variant189,
    Variant190,
    Variant191,
    Variant192,
    Variant193,
    Variant194,
    Variant195,
    Variant196,
    Variant197,
    Variant198,
    Variant199,
    Variant200,
    Variant201,
    Variant202,
    Variant203,
    Variant204,
    Variant205,
    Variant206,
    Variant207,
    Variant208,
    Variant209,
    Variant210,
    Variant211,
    Variant212,
    Variant213,
    Variant214,
    Variant215,
    Variant216,
    Variant217,
    Variant218,
    Variant219,
    Variant220,
    Variant221,
    Variant222,
    Variant223,
    Variant224,
    Variant225,
    Variant226,
    Variant227,
    Variant228,
    Variant229,
    Variant230,
    Variant231,
    Variant232,
    Variant233,
    Variant234,
    Variant235,
    Variant236,
    Variant237,
    Variant238,
    Variant239,
    Variant240,
    Variant241,
    Variant242,
    Variant243,
    Variant244,
    Variant245,
    Variant246,
    Variant247,
    Variant248,
    Variant249,
    Variant250,
    Variant251,
    Variant252,
    Variant253,
    Variant254,
    Variant255(bool),
}

//
// Unaligned errors
//

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum Unaligned1 {
    A,
}

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u16)]
enum Unaligned2 {
    A,
}

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i16)]
enum Unaligned3 {
    A,
}

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u32)]
enum Unaligned4 {
    A,
}

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i32)]
enum Unaligned5 {
    A,
}

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u64)]
enum Unaligned6 {
    A,
}

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i64)]
enum Unaligned7 {
    A,
}

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(usize)]
enum Unaligned8 {
    A,
}

#[derive(Unaligned)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(u8)] or #[repr(i8)] attribute in order to guarantee this type's alignment
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(isize)]
enum Unaligned9 {
    A,
}

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8, align(2))]
//~[msrv, stable, nightly]^ ERROR: cannot derive `Unaligned` on type with alignment greater than 1
enum Unaligned10 {
    A,
}

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i8, align(2))]
//~[msrv, stable, nightly]^ ERROR: cannot derive `Unaligned` on type with alignment greater than 1
enum Unaligned11 {
    A,
}

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(align(1), align(2))]
//~[msrv, stable, nightly]^ ERROR: this conflicts with another representation hint
enum Unaligned12 {
    A,
}

#[derive(Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(align(2), align(4))]
//~[msrv, stable, nightly]^ ERROR: this conflicts with another representation hint
enum Unaligned13 {
    A,
}

//
// IntoBytes errors
//

#[derive(IntoBytes)]
//~[msrv]^ ERROR: the trait bound `(): PaddingFree<IntoBytes1, 1_usize>` is not satisfied
//~[stable, nightly]^^ ERROR: `IntoBytes1` has 1 total byte(s) of padding
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum IntoBytes1 {
    A,
    B(u8),
}

#[derive(IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, align(4))]
struct Align4IntoBytes(u32);

#[derive(IntoBytes)]
//~[msrv]^ ERROR: the trait bound `(): PaddingFree<IntoBytes2, 3_usize>` is not satisfied
//~[stable, nightly]^^ ERROR: `IntoBytes2` has 3 total byte(s) of padding
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum IntoBytes2 {
    A(Align4IntoBytes),
}

#[derive(IntoBytes)]
//~[msrv]^ ERROR: the trait bound `(): PaddingFree<IntoBytes3, 2_usize>` is not satisfied
//~[stable, nightly]^^ ERROR: `IntoBytes3` has 2 total byte(s) of padding
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u32)]
enum IntoBytes3 {
    A(u32),
    B(u16),
}

#[derive(IntoBytes)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
#[zerocopy(crate = "zerocopy_renamed")]
enum IntoBytes4 {
    A(u32),
    B(u16),
}

#[derive(IntoBytes)]
//~[msrv, stable, nightly]^ ERROR: must have #[repr(C)] or #[repr(Int)] attribute in order to guarantee this type's memory layout
#[zerocopy(crate = "zerocopy_renamed")]
enum IntoBytes5 {
    A(u32),
}

#[derive(IntoBytes)]
//~[msrv, stable, nightly]^ ERROR: generic `Self` types are currently not permitted in anonymous constants
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum IntoBytes6<T> {
    A(T),
    //~[msrv, stable, nightly]^ ERROR: generic parameters may not be used in const operations
}

#[derive(IntoBytes)]
//~[msrv]^ ERROR: the trait bound `(): PaddingFree<IntoBytes7, 1_usize>` is not satisfied
//~[stable, nightly]^^ ERROR: `IntoBytes7` has 1 total byte(s) of padding
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8, align(2))]
enum IntoBytes7 {
    A,
}
