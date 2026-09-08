// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(dead_code)]

use zerovec::ZeroVec;

#[zerovec::make_ule(SparseEnumULE)]
#[zerovec::skip_derive(ZeroMapKV)]
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Ord, PartialOrd)]
enum SparseEnum {
    One = 1,
    Three = 3,
}

#[zerovec::make_ule(MissingZeroEnumULE)]
#[zerovec::skip_derive(ZeroMapKV)]
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Ord, PartialOrd)]
enum MissingZeroEnum {
    One = 1,
}

#[zerovec::make_ule(NonZeroContiguousEnumULE)]
#[zerovec::skip_derive(ZeroMapKV)]
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Ord, PartialOrd)]
enum NonZeroContiguousEnum {
    Two = 2,
    Three = 3,
}

#[zerovec::make_ule(ComplexSparseEnumULE)]
#[zerovec::skip_derive(ZeroMapKV)]
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Ord, PartialOrd)]
enum ComplexSparseEnum {
    A = 1,
    B = 2,
    C = 10,
    D = 11,
    E = 20,
}

#[test]
fn make_ule_sparse_enum_parse_rejects_invalid_discriminant() {
    let vec = ZeroVec::<SparseEnum>::parse_bytes(&[2]);
    assert!(vec.is_err(), "Should reject byte 2");

    let vec = ZeroVec::<SparseEnum>::parse_bytes(&[1]);
    assert!(vec.is_ok(), "Should accept byte 1");

    let vec = ZeroVec::<SparseEnum>::parse_bytes(&[3]);
    assert!(vec.is_ok(), "Should accept byte 3");
}

#[test]
fn make_ule_new_from_u8_rejects_invalid_discriminant() {
    let value = MissingZeroEnum::new_from_u8(0);
    assert!(value.is_none(), "Should reject 0");

    let value = MissingZeroEnum::new_from_u8(1);
    assert!(value.is_some(), "Should accept 1");
}

#[test]
fn make_ule_non_zero_contiguous_enum() {
    let vec = ZeroVec::<NonZeroContiguousEnum>::parse_bytes(&[1]);
    assert!(vec.is_err(), "Should reject byte 1");

    let vec = ZeroVec::<NonZeroContiguousEnum>::parse_bytes(&[2]);
    assert!(vec.is_ok(), "Should accept byte 2");

    let vec = ZeroVec::<NonZeroContiguousEnum>::parse_bytes(&[3]);
    assert!(vec.is_ok(), "Should accept byte 3");

    let vec = ZeroVec::<NonZeroContiguousEnum>::parse_bytes(&[4]);
    assert!(vec.is_err(), "Should reject byte 4");

    assert!(
        NonZeroContiguousEnum::new_from_u8(1).is_none(),
        "Should reject 1"
    );
    assert!(
        NonZeroContiguousEnum::new_from_u8(2).is_some(),
        "Should accept 2"
    );
    assert!(
        NonZeroContiguousEnum::new_from_u8(3).is_some(),
        "Should accept 3"
    );
    assert!(
        NonZeroContiguousEnum::new_from_u8(4).is_none(),
        "Should reject 4"
    );
}

#[test]
fn make_ule_complex_sparse_enum() {
    let valid = &[1, 2, 10, 11, 20];
    for &v in valid {
        assert!(
            ZeroVec::<ComplexSparseEnum>::parse_bytes(&[v]).is_ok(),
            "Should accept byte {}",
            v
        );
        assert!(
            ComplexSparseEnum::new_from_u8(v).is_some(),
            "Should accept {}",
            v
        );
    }

    let invalid = &[0, 3, 9, 12, 19, 21];
    for &i in invalid {
        assert!(
            ZeroVec::<ComplexSparseEnum>::parse_bytes(&[i]).is_err(),
            "Should reject byte {}",
            i
        );
        assert!(
            ComplexSparseEnum::new_from_u8(i).is_none(),
            "Should reject {}",
            i
        );
    }
}
