// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use zerotrie::ZeroTrieSimpleAscii;

/// A trie that contains the powers.
const POWERS_TRIE: ZeroTrieSimpleAscii<[u8; 64]> = ZeroTrieSimpleAscii::from_sorted_str_tuples(&[
    ("cubic", 3),
    ("pow1", 1),
    ("pow10", 10),
    ("pow11", 11),
    ("pow12", 12),
    ("pow13", 13),
    ("pow14", 14),
    ("pow15", 15),
    ("pow2", 2),
    ("pow3", 3),
    ("pow4", 4),
    ("pow5", 5),
    ("pow6", 6),
    ("pow7", 7),
    ("pow8", 8),
    ("pow9", 9),
    ("square", 2),
]);

// TODO: consider returning Option<(u8, &str)> instead of (1, part) for the case when the power is not found.
// TODO: complete all the cases for the powers.
// TODO: consider using a trie for the powers.
/// Extracts the power from the given CLDR ID part.
///     - If the power is not found, the function returns (1, part).
///     - If the power is found, the function will return (power, part without the string of the power).
pub fn get_power(part: &[u8]) -> (u8, &[u8]) {
    let mut cursor = POWERS_TRIE.cursor();
    let mut longest_match = (1, part);
    for (i, &b) in part.iter().enumerate() {
        cursor.step(b);
        if cursor.is_empty() {
            break;
        }
        if let Some(value) = cursor.take_value() {
            longest_match = (value as u8, &part[i + 1..]);
        }
    }
    longest_match
}
