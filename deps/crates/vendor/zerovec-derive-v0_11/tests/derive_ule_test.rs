// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use zerovec::ule::AsULE;
use zerovec::ule::UleError;
use zerovec::ule::ULE;
use zerovec::ZeroSlice;
use zerovec::ZeroVec;

#[derive(Debug, Copy, Clone)]
struct MyTuple(u16, char);

#[derive(ULE, Copy, Clone)]
#[repr(C, packed)]
struct MyTupleULE(<u16 as AsULE>::ULE, <char as AsULE>::ULE);

impl AsULE for MyTuple {
    type ULE = MyTupleULE;
    fn to_unaligned(self) -> Self::ULE {
        MyTupleULE(self.0.to_unaligned(), self.1.to_unaligned())
    }
    fn from_unaligned(ule: Self::ULE) -> Self {
        MyTuple(u16::from_unaligned(ule.0), char::from_unaligned(ule.1))
    }
}

#[zerovec::make_ule(MyStructULE)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
struct MyStruct {
    a: u16,
    b: char,
}

#[test]
fn test_vec_validation() {
    // Construct a valid ZeroVec<MyTuple>
    let zv = ZeroVec::alloc_from_slice(&[MyTuple(1, 'a'), MyTuple(2, 'ß'), MyTuple(3, 'ç')]);
    // Should succeed
    ZeroSlice::<MyTuple>::parse_bytes(zv.as_bytes()).unwrap();

    // Construct invalid bytes: 0xff are invalid bytes for char
    let mut invalid_bytes = zv.as_bytes().to_vec();
    // 0xff is invalid for char, which starts at index 7
    invalid_bytes[7] = 0xff;
    invalid_bytes[8] = 0xff;
    invalid_bytes[9] = 0xff;
    // Should fail
    let err = ZeroSlice::<MyTuple>::parse_bytes(&invalid_bytes).unwrap_err();
    assert!(matches!(err, UleError::ParseError { .. }));
}

#[test]
fn test_make_ule_vec_validation() {
    // Construct a valid ZeroVec<MyStruct>
    let zv = ZeroVec::alloc_from_slice(&[
        MyStruct { a: 1, b: 'a' },
        MyStruct { a: 2, b: 'ß' },
        MyStruct { a: 3, b: 'ç' },
    ]);
    // Should succeed
    ZeroSlice::<MyStruct>::parse_bytes(zv.as_bytes()).unwrap();

    // Construct invalid bytes: 0xff are invalid bytes for char
    let mut invalid_bytes = zv.as_bytes().to_vec();
    // 0xff is invalid for char, which starts at index 7
    invalid_bytes[7] = 0xff;
    invalid_bytes[8] = 0xff;
    invalid_bytes[9] = 0xff;
    // Should fail
    let err = ZeroSlice::<MyStruct>::parse_bytes(&invalid_bytes).unwrap_err();
    assert!(matches!(err, UleError::ParseError { .. }));
}
