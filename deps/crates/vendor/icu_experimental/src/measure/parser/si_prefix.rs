// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use zerotrie::ZeroTrieSimpleAscii;

use crate::measure::provider::si_prefix::{Base, SiPrefix};

/// The offset of the SI prefixes.
/// NOTE:
///     The offset is added to the power of the decimal SI prefixes in order to avoid negative powers.
///     Therefore, if there is a prefix with power more than -30, the offset should be increased.
const SI_PREFIXES_OFFSET: u8 = 30;

/// A trie that contains the decimal SI prefixes.
const DECIMAL_PREFIXES_TRIE: ZeroTrieSimpleAscii<[u8; 167]> =
    ZeroTrieSimpleAscii::from_sorted_str_tuples(&[
        ("atto", (-18 + SI_PREFIXES_OFFSET as i16) as usize),
        ("centi", (-2 + SI_PREFIXES_OFFSET as i16) as usize),
        ("deca", (1 + SI_PREFIXES_OFFSET) as usize),
        ("deci", (-1 + SI_PREFIXES_OFFSET as i16) as usize),
        ("exa", (18 + SI_PREFIXES_OFFSET) as usize),
        ("femto", (-15 + SI_PREFIXES_OFFSET as i16) as usize),
        ("giga", (9 + SI_PREFIXES_OFFSET) as usize),
        ("hecto", (2 + SI_PREFIXES_OFFSET) as usize),
        ("kilo", (3 + SI_PREFIXES_OFFSET) as usize),
        ("mega", (6 + SI_PREFIXES_OFFSET) as usize),
        ("micro", (-6 + SI_PREFIXES_OFFSET as i16) as usize),
        ("milli", (-3 + SI_PREFIXES_OFFSET as i16) as usize),
        ("nano", (-9 + SI_PREFIXES_OFFSET as i16) as usize),
        ("peta", (15 + SI_PREFIXES_OFFSET) as usize),
        ("pico", (-12 + SI_PREFIXES_OFFSET as i16) as usize),
        ("quecto", (-30 + SI_PREFIXES_OFFSET as i16) as usize),
        ("quetta", (30 + SI_PREFIXES_OFFSET) as usize),
        ("ronna", (27 + SI_PREFIXES_OFFSET) as usize),
        ("ronto", (-27 + SI_PREFIXES_OFFSET as i16) as usize),
        ("tera", (12 + SI_PREFIXES_OFFSET) as usize),
        ("yocto", (-24 + SI_PREFIXES_OFFSET as i16) as usize),
        ("yotta", (24 + SI_PREFIXES_OFFSET) as usize),
        ("zepto", (-21 + SI_PREFIXES_OFFSET as i16) as usize),
        ("zetta", (21 + SI_PREFIXES_OFFSET) as usize),
    ]);

// TODO: consider returning Option<(i8, &str)> instead of (0, part) for the case when the prefix is not found.
// TODO: consider using a trie for the prefixes.
// TODO: complete all the cases for the prefixes.
/// Extracts the SI prefix of base 10.
/// NOTE:
///    if the prefix is found, the function will return (power, part without the prefix).
///    if the prefix is not found, the function will return (0, part).
fn get_si_prefix_base_ten(part: &[u8]) -> (i8, &[u8]) {
    let mut cursor = DECIMAL_PREFIXES_TRIE.cursor();

    let mut longest_match = (0_i8, part);
    for (i, &b) in part.iter().enumerate() {
        cursor.step(b);
        if cursor.is_empty() {
            break;
        }
        if let Some(value) = cursor.take_value() {
            let power = value as i8 - SI_PREFIXES_OFFSET as i8;
            longest_match = (power, &part[i + 1..]);
        }
    }
    longest_match
}

/// A trie that contains the binary SI prefixes.
const BINARY_TRIE: ZeroTrieSimpleAscii<[u8; 55]> = ZeroTrieSimpleAscii::from_sorted_str_tuples(&[
    ("exbi", 60),
    ("gibi", 30),
    ("kibi", 10),
    ("mebi", 20),
    ("pebi", 50),
    ("tebi", 40),
    ("yobi", 80),
    ("zebi", 70),
]);

// TODO: consider returning Option<(i8, &str)> instead of (0, part) for the case when the prefix is not found.
// TODO: consider using a trie for the prefixes.
// TODO: complete all the cases for the prefixes.
/// Extracts the SI prefix of base 2.
/// NOTE:
///     if the prefix is found, the function will return (power, part without the prefix).
///     if the prefix is not found, the function will return (0, part).
fn get_si_prefix_base_two(part: &[u8]) -> (i8, &[u8]) {
    let mut cursor = BINARY_TRIE.cursor();
    let mut longest_match = (0, part);
    for (i, &b) in part.iter().enumerate() {
        cursor.step(b);
        if cursor.is_empty() {
            break;
        }
        if let Some(value) = cursor.take_value() {
            longest_match = (value as i8, &part[i + 1..]);
        }
    }
    longest_match
}

// TODO: complete all the cases for the prefixes.
// TODO: consider using a trie for the prefixes.
/// Extracts the SI prefix.
/// NOTE:
///    if the prefix is found, the function will return (`SiPrefix`, part without the prefix string).
///    if the prefix is not found, the function will return (`SiPrefix { power: 0, base: Base::Decimal }`, part).
pub fn get_si_prefix(part: &[u8]) -> (SiPrefix, &[u8]) {
    let (si_prefix_base_10, part) = get_si_prefix_base_ten(part);
    if si_prefix_base_10 != 0 {
        return (
            SiPrefix {
                power: si_prefix_base_10,
                base: Base::Decimal,
            },
            part,
        );
    }

    let (si_prefix_base_2, part) = get_si_prefix_base_two(part);
    if si_prefix_base_2 != 0 {
        return (
            SiPrefix {
                power: si_prefix_base_2,
                base: Base::Binary,
            },
            part,
        );
    }

    (
        SiPrefix {
            power: 0,
            base: Base::Decimal,
        },
        part,
    )
}
