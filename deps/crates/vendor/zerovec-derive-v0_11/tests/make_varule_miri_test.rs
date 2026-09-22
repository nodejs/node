// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use zerovec::ule::{AsULE, VarULE};

// https://github.com/unicode-org/icu4x/issues/6723
#[zerovec::make_varule(CustomVarULE)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
struct CustomVar<'a> {
    a: u32,
    b: &'a str,
}

#[test]
fn test_make_varule_from_bytes_unchecked_miri() {
    // https://github.com/unicode-org/icu4x/issues/6723
    let custom = CustomVar { a: 12345, b: "x" };
    let boxed: Box<CustomVarULE> = zerovec::ule::encode_varule_to_box(&custom);
    let bytes = boxed.as_bytes();
    let ule = unsafe { CustomVarULE::from_bytes_unchecked(bytes) };
    assert_eq!(u32::from_unaligned(ule.a), 12345);
    assert_eq!(&ule.b, "x");
}
