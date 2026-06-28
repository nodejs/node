// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module defines implementations for code-based transliterators that are part of
//! transform rules.

use crate::transliterate::transliterator::replaceable::{Forward, Replaceable, Utf8Matcher};

/// A transliterator that replaces every character with its `case`-case hexadecimal representation,
/// 0-padded to `min_length`, and surrounded by `prefix` and `suffix`.
#[derive(Debug)]
pub(super) struct HexTransliterator {
    prefix: &'static str,
    suffix: &'static str,
    min_length: u8,
    case: Case,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(super) enum Case {
    Upper,
    Lower,
}

impl HexTransliterator {
    pub(super) fn new(
        prefix: &'static str,
        suffix: &'static str,
        min_length: u8,
        case: Case,
    ) -> Self {
        Self {
            prefix,
            suffix,
            min_length,
            case,
        }
    }

    pub(super) fn transliterate(&self, mut rep: Replaceable) {
        while !rep.is_finished() {
            let mut matcher = rep.start_match();
            // Thought: ok this fully specified path is annoying, maybe a separate API surface is
            // better for Forward vs Reverse matching.
            let c = Utf8Matcher::<Forward>::next_char(&matcher);
            // there must always be a char, because we just checked that `rep` is not finished yet.
            let c = c.unwrap();
            Utf8Matcher::<Forward>::match_and_consume_char(&mut matcher, c);
            let mut dest = matcher.finish_match();

            let c_u32 = c as u32;
            // rounding-up division by 4
            let length = (u32::BITS - c_u32.leading_zeros()).div_ceil(4);
            let padding = self.min_length.saturating_sub(length as u8);
            dest.apply_size_hint(
                self.prefix.len() + padding as usize + length as usize + self.suffix.len(),
            );

            dest.push_str(self.prefix);
            for _ in 0..padding {
                dest.push_str("0");
            }
            let mut remaining_c = c_u32;
            // temporary buffer because forward iteration through a u32's bytes is easier and
            // we need the reverse order
            let mut buf = [0; 6];
            for slot in buf.iter_mut() {
                if c_u32 == 0 {
                    break;
                }
                *slot = match remaining_c & 0xF {
                    x @ 0x0..=0x9 => b'0' + x as u8,
                    x @ 0xA..=0xF if self.case == Case::Lower => b'a' + (x - 0xA) as u8,
                    x => b'A' + (x - 0xA) as u8,
                };
                remaining_c >>= 4;
            }
            // only `length` hex digits are actually from the char
            for c in buf[..length as usize].iter().rev() {
                dest.push(*c as char);
            }
            dest.push_str(self.suffix);
        }
    }
}
