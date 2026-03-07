// Copyright 2013-2016 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core::{mem, ops};

/// Represents a set of characters or bytes in the ASCII range.
///
/// This is used in [`percent_encode`] and [`utf8_percent_encode`].
/// This is similar to [percent-encode sets](https://url.spec.whatwg.org/#percent-encoded-bytes).
///
/// Use the `add` method of an existing set to define a new set. For example:
///
/// [`percent_encode`]: crate::percent_encode
/// [`utf8_percent_encode`]: crate::utf8_percent_encode
///
/// ```
/// use percent_encoding::{AsciiSet, CONTROLS};
///
/// /// https://url.spec.whatwg.org/#fragment-percent-encode-set
/// const FRAGMENT: &AsciiSet = &CONTROLS.add(b' ').add(b'"').add(b'<').add(b'>').add(b'`');
/// ```
#[derive(Debug, PartialEq, Eq)]
pub struct AsciiSet {
    mask: [Chunk; ASCII_RANGE_LEN / BITS_PER_CHUNK],
}

type Chunk = u32;

const ASCII_RANGE_LEN: usize = 0x80;

const BITS_PER_CHUNK: usize = 8 * mem::size_of::<Chunk>();

impl AsciiSet {
    /// An empty set.
    pub const EMPTY: Self = Self {
        mask: [0; ASCII_RANGE_LEN / BITS_PER_CHUNK],
    };

    /// Called with UTF-8 bytes rather than code points.
    /// Not used for non-ASCII bytes.
    pub(crate) const fn contains(&self, byte: u8) -> bool {
        let chunk = self.mask[byte as usize / BITS_PER_CHUNK];
        let mask = 1 << (byte as usize % BITS_PER_CHUNK);
        (chunk & mask) != 0
    }

    pub(crate) fn should_percent_encode(&self, byte: u8) -> bool {
        !byte.is_ascii() || self.contains(byte)
    }

    pub const fn add(&self, byte: u8) -> Self {
        let mut mask = self.mask;
        mask[byte as usize / BITS_PER_CHUNK] |= 1 << (byte as usize % BITS_PER_CHUNK);
        Self { mask }
    }

    pub const fn remove(&self, byte: u8) -> Self {
        let mut mask = self.mask;
        mask[byte as usize / BITS_PER_CHUNK] &= !(1 << (byte as usize % BITS_PER_CHUNK));
        Self { mask }
    }

    /// Return the union of two sets.
    pub const fn union(&self, other: Self) -> Self {
        let mask = [
            self.mask[0] | other.mask[0],
            self.mask[1] | other.mask[1],
            self.mask[2] | other.mask[2],
            self.mask[3] | other.mask[3],
        ];
        Self { mask }
    }

    /// Return the negation of the set.
    pub const fn complement(&self) -> Self {
        let mask = [!self.mask[0], !self.mask[1], !self.mask[2], !self.mask[3]];
        Self { mask }
    }
}

impl ops::Add for AsciiSet {
    type Output = Self;

    fn add(self, other: Self) -> Self {
        self.union(other)
    }
}

impl ops::Not for AsciiSet {
    type Output = Self;

    fn not(self) -> Self {
        self.complement()
    }
}

/// The set of 0x00 to 0x1F (C0 controls), and 0x7F (DEL).
///
/// Note that this includes the newline and tab characters, but not the space 0x20.
///
/// <https://url.spec.whatwg.org/#c0-control-percent-encode-set>
pub const CONTROLS: &AsciiSet = &AsciiSet {
    mask: [
        !0_u32, // C0: 0x00 to 0x1F (32 bits set)
        0,
        0,
        1 << (0x7F_u32 % 32), // DEL: 0x7F (one bit set)
    ],
};

macro_rules! static_assert {
    ($( $bool: expr, )+) => {
        fn _static_assert() {
            $(
                let _ = mem::transmute::<[u8; $bool as usize], u8>;
            )+
        }
    }
}

static_assert! {
    CONTROLS.contains(0x00),
    CONTROLS.contains(0x1F),
    !CONTROLS.contains(0x20),
    !CONTROLS.contains(0x7E),
    CONTROLS.contains(0x7F),
}

/// Everything that is not an ASCII letter or digit.
///
/// This is probably more eager than necessary in any context.
pub const NON_ALPHANUMERIC: &AsciiSet = &CONTROLS
    .add(b' ')
    .add(b'!')
    .add(b'"')
    .add(b'#')
    .add(b'$')
    .add(b'%')
    .add(b'&')
    .add(b'\'')
    .add(b'(')
    .add(b')')
    .add(b'*')
    .add(b'+')
    .add(b',')
    .add(b'-')
    .add(b'.')
    .add(b'/')
    .add(b':')
    .add(b';')
    .add(b'<')
    .add(b'=')
    .add(b'>')
    .add(b'?')
    .add(b'@')
    .add(b'[')
    .add(b'\\')
    .add(b']')
    .add(b'^')
    .add(b'_')
    .add(b'`')
    .add(b'{')
    .add(b'|')
    .add(b'}')
    .add(b'~');

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn add_op() {
        let left = AsciiSet::EMPTY.add(b'A');
        let right = AsciiSet::EMPTY.add(b'B');
        let expected = AsciiSet::EMPTY.add(b'A').add(b'B');
        assert_eq!(left + right, expected);
    }

    #[test]
    fn not_op() {
        let set = AsciiSet::EMPTY.add(b'A').add(b'B');
        let not_set = !set;
        assert!(!not_set.contains(b'A'));
        assert!(not_set.contains(b'C'));
    }

    /// This test ensures that we can get the union of two sets as a constant value, which is
    /// useful for defining sets in a modular way.
    #[test]
    fn union() {
        const A: AsciiSet = AsciiSet::EMPTY.add(b'A');
        const B: AsciiSet = AsciiSet::EMPTY.add(b'B');
        const UNION: AsciiSet = A.union(B);
        const EXPECTED: AsciiSet = AsciiSet::EMPTY.add(b'A').add(b'B');
        assert_eq!(UNION, EXPECTED);
    }

    /// This test ensures that we can get the complement of a set as a constant value, which is
    /// useful for defining sets in a modular way.
    #[test]
    fn complement() {
        const BOTH: AsciiSet = AsciiSet::EMPTY.add(b'A').add(b'B');
        const COMPLEMENT: AsciiSet = BOTH.complement();
        assert!(!COMPLEMENT.contains(b'A'));
        assert!(!COMPLEMENT.contains(b'B'));
        assert!(COMPLEMENT.contains(b'C'));
    }
}
