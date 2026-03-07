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

#[derive(Eq, PartialEq, Debug, imp::Immutable, imp::KnownLayout, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum Foo {
    A,
}

util_assert_impl_all!(Foo: imp::TryFromBytes);

#[test]
fn test_foo() {
    imp::assert_eq!(<Foo as imp::TryFromBytes>::try_read_from_bytes(&[0]), imp::Ok(Foo::A));
    imp::assert!(<Foo as imp::TryFromBytes>::try_read_from_bytes(&[]).is_err());
    imp::assert!(<Foo as imp::TryFromBytes>::try_read_from_bytes(&[1]).is_err());
    imp::assert!(<Foo as imp::TryFromBytes>::try_read_from_bytes(&[0, 0]).is_err());
}

#[derive(Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u16)]
enum Bar {
    A = 0,
}

util_assert_impl_all!(Bar: imp::TryFromBytes);

#[test]
fn test_bar() {
    imp::assert_eq!(<Bar as imp::TryFromBytes>::try_read_from_bytes(&[0, 0]), imp::Ok(Bar::A));
    imp::assert!(<Bar as imp::TryFromBytes>::try_read_from_bytes(&[]).is_err());
    imp::assert!(<Bar as imp::TryFromBytes>::try_read_from_bytes(&[0]).is_err());
    imp::assert!(<Bar as imp::TryFromBytes>::try_read_from_bytes(&[0, 1]).is_err());
    imp::assert!(<Bar as imp::TryFromBytes>::try_read_from_bytes(&[0, 0, 0]).is_err());
}

#[derive(Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u32)]
enum Baz {
    A = 1,
    B = 0,
}

util_assert_impl_all!(Baz: imp::TryFromBytes);

#[test]
fn test_baz() {
    imp::assert_eq!(
        <Baz as imp::TryFromBytes>::try_read_from_bytes(imp::IntoBytes::as_bytes(&1u32)),
        imp::Ok(Baz::A)
    );
    imp::assert_eq!(
        <Baz as imp::TryFromBytes>::try_read_from_bytes(imp::IntoBytes::as_bytes(&0u32)),
        imp::Ok(Baz::B)
    );
    imp::assert!(<Baz as imp::TryFromBytes>::try_read_from_bytes(&[]).is_err());
    imp::assert!(<Baz as imp::TryFromBytes>::try_read_from_bytes(&[0]).is_err());
    imp::assert!(<Baz as imp::TryFromBytes>::try_read_from_bytes(&[0, 0]).is_err());
    imp::assert!(<Baz as imp::TryFromBytes>::try_read_from_bytes(&[0, 0, 0]).is_err());
    imp::assert!(<Baz as imp::TryFromBytes>::try_read_from_bytes(&[0, 0, 0, 0, 0]).is_err());
}

// Test hygiene - make sure that `i8` being shadowed doesn't cause problems for
// the code emitted by the derive.
type i8 = bool;

const THREE: ::core::primitive::i8 = 3;

#[derive(Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i8)]
enum Blah {
    A = 1,
    B = 0,
    C = 1 + 2,
    D = 3 + THREE,
}

util_assert_impl_all!(Blah: imp::TryFromBytes);

#[test]
fn test_blah() {
    imp::assert_eq!(
        <Blah as imp::TryFromBytes>::try_read_from_bytes(imp::IntoBytes::as_bytes(&1i8)),
        imp::Ok(Blah::A)
    );
    imp::assert_eq!(
        <Blah as imp::TryFromBytes>::try_read_from_bytes(imp::IntoBytes::as_bytes(&0i8)),
        imp::Ok(Blah::B)
    );
    imp::assert_eq!(
        <Blah as imp::TryFromBytes>::try_read_from_bytes(imp::IntoBytes::as_bytes(&3i8)),
        imp::Ok(Blah::C)
    );
    imp::assert_eq!(
        <Blah as imp::TryFromBytes>::try_read_from_bytes(imp::IntoBytes::as_bytes(&6i8)),
        imp::Ok(Blah::D)
    );
    imp::assert!(<Blah as imp::TryFromBytes>::try_read_from_bytes(&[]).is_err());
    imp::assert!(<Blah as imp::TryFromBytes>::try_read_from_bytes(&[4]).is_err());
    imp::assert!(<Blah as imp::TryFromBytes>::try_read_from_bytes(&[0, 0]).is_err());
}

#[derive(
    Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes, imp::IntoBytes,
)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum FieldlessButNotUnitOnly {
    A,
    B(),
    C {},
}

#[test]
fn test_fieldless_but_not_unit_only() {
    const SIZE: usize = ::core::mem::size_of::<FieldlessButNotUnitOnly>();
    let disc: [u8; SIZE] = ::zerocopy_renamed::transmute!(FieldlessButNotUnitOnly::A);
    imp::assert_eq!(
        <FieldlessButNotUnitOnly as imp::TryFromBytes>::try_read_from_bytes(&disc[..]),
        imp::Ok(FieldlessButNotUnitOnly::A)
    );
    let disc: [u8; SIZE] = ::zerocopy_renamed::transmute!(FieldlessButNotUnitOnly::B());
    imp::assert_eq!(
        <FieldlessButNotUnitOnly as imp::TryFromBytes>::try_read_from_bytes(&disc[..]),
        imp::Ok(FieldlessButNotUnitOnly::B())
    );
    let disc: [u8; SIZE] = ::zerocopy_renamed::transmute!(FieldlessButNotUnitOnly::C {});
    imp::assert_eq!(
        <FieldlessButNotUnitOnly as imp::TryFromBytes>::try_read_from_bytes(&disc[..]),
        imp::Ok(FieldlessButNotUnitOnly::C {})
    );
    imp::assert!(<FieldlessButNotUnitOnly as imp::TryFromBytes>::try_read_from_bytes(
        &[0xFF; SIZE][..]
    )
    .is_err());
}

#[derive(
    Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes, imp::IntoBytes,
)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum WeirdDiscriminants {
    A = -7,
    B,
    C = 33,
}

#[test]
fn test_weird_discriminants() {
    const SIZE: usize = ::core::mem::size_of::<WeirdDiscriminants>();
    let disc: [u8; SIZE] = ::zerocopy_renamed::transmute!(WeirdDiscriminants::A);
    imp::assert_eq!(
        <WeirdDiscriminants as imp::TryFromBytes>::try_read_from_bytes(&disc[..]),
        imp::Ok(WeirdDiscriminants::A)
    );
    let disc: [u8; SIZE] = ::zerocopy_renamed::transmute!(WeirdDiscriminants::B);
    imp::assert_eq!(
        <WeirdDiscriminants as imp::TryFromBytes>::try_read_from_bytes(&disc[..]),
        imp::Ok(WeirdDiscriminants::B)
    );
    let disc: [u8; SIZE] = ::zerocopy_renamed::transmute!(WeirdDiscriminants::C);
    imp::assert_eq!(
        <WeirdDiscriminants as imp::TryFromBytes>::try_read_from_bytes(&disc[..]),
        imp::Ok(WeirdDiscriminants::C)
    );
    imp::assert!(
        <WeirdDiscriminants as imp::TryFromBytes>::try_read_from_bytes(&[0xFF; SIZE][..]).is_err()
    );
}

// Technically non-portable since this is only `IntoBytes` if the discriminant
// is an `i32` or `u32`, but we'll cross that bridge when we get to it...
#[derive(
    Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes, imp::IntoBytes,
)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum HasFields {
    A(u32),
    B { foo: ::core::num::NonZeroU32 },
}

#[test]
fn test_has_fields() {
    const SIZE: usize = ::core::mem::size_of::<HasFields>();

    let bytes: [u8; SIZE] = ::zerocopy_renamed::transmute!(HasFields::A(10));
    imp::assert_eq!(
        <HasFields as imp::TryFromBytes>::try_read_from_bytes(&bytes[..]),
        imp::Ok(HasFields::A(10)),
    );

    let bytes: [u8; SIZE] = ::zerocopy_renamed::transmute!(HasFields::B {
        foo: ::core::num::NonZeroU32::new(123456).unwrap()
    });
    imp::assert_eq!(
        <HasFields as imp::TryFromBytes>::try_read_from_bytes(&bytes[..]),
        imp::Ok(HasFields::B { foo: ::core::num::NonZeroU32::new(123456).unwrap() }),
    );
}

#[derive(Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, align(16))]
enum HasFieldsAligned {
    A(u32),
    B { foo: ::core::num::NonZeroU32 },
}

util_assert_impl_all!(HasFieldsAligned: imp::TryFromBytes);

#[test]
fn test_has_fields_aligned() {
    const SIZE: usize = ::core::mem::size_of::<HasFieldsAligned>();

    #[derive(imp::IntoBytes)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(C)]
    struct BytesOfHasFieldsAligned {
        has_fields: HasFields,
        padding: [u8; 8],
    }

    let wrap = |has_fields| BytesOfHasFieldsAligned { has_fields, padding: [0; 8] };

    let bytes: [u8; SIZE] = ::zerocopy_renamed::transmute!(wrap(HasFields::A(10)));
    imp::assert_eq!(
        <HasFieldsAligned as imp::TryFromBytes>::try_read_from_bytes(&bytes[..]),
        imp::Ok(HasFieldsAligned::A(10)),
    );

    let bytes: [u8; SIZE] = ::zerocopy_renamed::transmute!(wrap(HasFields::B {
        foo: ::core::num::NonZeroU32::new(123456).unwrap()
    }));
    imp::assert_eq!(
        <HasFieldsAligned as imp::TryFromBytes>::try_read_from_bytes(&bytes[..]),
        imp::Ok(HasFieldsAligned::B { foo: ::core::num::NonZeroU32::new(123456).unwrap() }),
    );
}

#[derive(
    Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes, imp::IntoBytes,
)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u32)]
enum HasFieldsPrimitive {
    A(u32),
    B { foo: ::core::num::NonZeroU32 },
}

#[test]
fn test_has_fields_primitive() {
    const SIZE: usize = ::core::mem::size_of::<HasFieldsPrimitive>();

    let bytes: [u8; SIZE] = ::zerocopy_renamed::transmute!(HasFieldsPrimitive::A(10));
    imp::assert_eq!(
        <HasFieldsPrimitive as imp::TryFromBytes>::try_read_from_bytes(&bytes[..]),
        imp::Ok(HasFieldsPrimitive::A(10)),
    );

    let bytes: [u8; SIZE] = ::zerocopy_renamed::transmute!(HasFieldsPrimitive::B {
        foo: ::core::num::NonZeroU32::new(123456).unwrap(),
    });
    imp::assert_eq!(
        <HasFieldsPrimitive as imp::TryFromBytes>::try_read_from_bytes(&bytes[..]),
        imp::Ok(HasFieldsPrimitive::B { foo: ::core::num::NonZeroU32::new(123456).unwrap() }),
    );
}

#[derive(Eq, PartialEq, Debug, imp::KnownLayout, imp::Immutable, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u32, align(16))]
enum HasFieldsPrimitiveAligned {
    A(u32),
    B { foo: ::core::num::NonZeroU32 },
}

util_assert_impl_all!(HasFieldsPrimitiveAligned: imp::TryFromBytes);

#[test]
fn test_has_fields_primitive_aligned() {
    const SIZE: usize = ::core::mem::size_of::<HasFieldsPrimitiveAligned>();

    #[derive(imp::IntoBytes)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(C)]
    struct BytesOfHasFieldsPrimitiveAligned {
        has_fields: HasFieldsPrimitive,
        padding: [u8; 8],
    }

    let wrap = |has_fields| BytesOfHasFieldsPrimitiveAligned { has_fields, padding: [0; 8] };

    let bytes: [u8; SIZE] = ::zerocopy_renamed::transmute!(wrap(HasFieldsPrimitive::A(10)));
    imp::assert_eq!(
        <HasFieldsPrimitiveAligned as imp::TryFromBytes>::try_read_from_bytes(&bytes[..]),
        imp::Ok(HasFieldsPrimitiveAligned::A(10)),
    );

    let bytes: [u8; SIZE] = ::zerocopy_renamed::transmute!(wrap(HasFieldsPrimitive::B {
        foo: ::core::num::NonZeroU32::new(123456).unwrap()
    }));
    imp::assert_eq!(
        <HasFieldsPrimitiveAligned as imp::TryFromBytes>::try_read_from_bytes(&bytes[..]),
        imp::Ok(HasFieldsPrimitiveAligned::B {
            foo: ::core::num::NonZeroU32::new(123456).unwrap()
        }),
    );
}

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(align(4), u32)]
enum HasReprAlignFirst {
    A,
    B,
}

util_assert_impl_all!(HasReprAlignFirst: imp::TryFromBytes);

#[derive(imp::KnownLayout, imp::TryFromBytes, imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum Complex {
    UnitLike,
    StructLike { a: u8, b: u16 },
    TupleLike(bool, char),
}

util_assert_impl_all!(Complex: imp::TryFromBytes);

#[derive(imp::KnownLayout, imp::TryFromBytes, imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum ComplexWithGenerics<X, Y> {
    UnitLike,
    StructLike { a: u8, b: X },
    TupleLike(bool, Y),
}

util_assert_impl_all!(ComplexWithGenerics<u16, char>: imp::TryFromBytes);

#[derive(imp::KnownLayout, imp::TryFromBytes, imp::Immutable)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum GenericWithLifetimes<'a, 'b, X: 'a, Y: 'b> {
    Foo(::core::marker::PhantomData<&'a X>),
    Bar(::core::marker::PhantomData<&'b Y>),
}

#[derive(Clone, Copy, imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
struct A;

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
enum B {
    A(A),
    A2 { a: A },
}

#[derive(imp::TryFromBytes)]
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
    Variant255,
}

#[test]
fn test_trivial_is_bit_valid() {
    // Though we don't derive `FromBytes`, `FooU8` *could* soundly implement
    // `FromBytes`. Therefore, `TryFromBytes` derive's `is_bit_valid` impl is
    // trivial - it unconditionally returns `true`.
    util_assert_not_impl_any!(FooU8: imp::FromBytes);
    util::test_trivial_is_bit_valid::<FooU8>();
}

#[deny(non_camel_case_types)]
mod issue_2051 {
    use super::*;

    // Test that the `non_camel_case_types` lint isn't triggered by generated
    // code.
    // Prevents regressions of #2051.
    #[repr(u32)]
    #[derive(imp::TryFromBytes)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[allow(non_camel_case_types)]
    pub enum Code {
        I32_ADD,
        I32_SUB,
        I32_MUL,
    }
}

#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum RawIdentifierVariant {
    r#type,
}

util_assert_impl_all!(RawIdentifierVariant: imp::TryFromBytes);
