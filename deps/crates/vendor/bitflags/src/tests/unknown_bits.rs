use super::*;

use crate::Flags;

#[test]
fn cases() {
    case(0, TestFlags::empty(), TestFlags::unknown_bits);
    case(0, TestFlags::A, TestFlags::unknown_bits);
    case(0, TestFlags::all(), TestFlags::unknown_bits);

    case(
        1 << 3,
        TestFlags::ABC | TestFlags::from_bits_retain(1 << 3),
        TestFlags::unknown_bits,
    );

    case(
        1 << 3,
        TestFlags::from_bits_retain(1 << 3),
        TestFlags::unknown_bits,
    );

    case(
        0b11111000,
        TestFlags::from_bits_retain(0b11111000),
        TestFlags::unknown_bits,
    );

    case(0, TestZero::empty(), TestZero::unknown_bits);

    case(
        0,
        TestExternal::from_bits_retain(0xFF),
        TestExternal::unknown_bits,
    );
}

#[track_caller]
fn case<T: Flags + std::fmt::Debug>(
    expected: T::Bits,
    value: T,
    inherent: impl FnOnce(&T) -> T::Bits,
) where
    T::Bits: std::fmt::Debug + PartialEq,
{
    assert_eq!(expected, inherent(&value), "{:?}.unknown_bits()", value);
    assert_eq!(
        expected,
        Flags::unknown_bits(&value),
        "Flags::unknown_bits({:?})",
        value,
    );
}
