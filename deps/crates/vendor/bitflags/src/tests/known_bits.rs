use super::*;

use crate::Flags;

#[test]
fn cases() {
    case(0, TestFlags::empty(), TestFlags::known_bits);
    case(1, TestFlags::A, TestFlags::known_bits);
    case(1 | 1 << 1 | 1 << 2, TestFlags::all(), TestFlags::known_bits);

    case(
        1 | 1 << 1 | 1 << 2,
        TestFlags::ABC | TestFlags::from_bits_retain(1 << 3),
        TestFlags::known_bits,
    );

    case(
        0,
        TestFlags::from_bits_retain(1 << 3),
        TestFlags::known_bits,
    );

    case(0, TestZero::empty(), TestZero::known_bits);

    case(
        0xFF,
        TestExternal::from_bits_retain(0xFF),
        TestExternal::known_bits,
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
    assert_eq!(expected, inherent(&value), "{:?}.known_bits()", value);
    assert_eq!(
        expected,
        Flags::known_bits(&value),
        "Flags::known_bits({:?})",
        value,
    );
}
