// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains internal data handling tools for the special-cased Greek uppercasing
//! code. The Greek uppercasing algorithm is code-driven, not user-data-driven, however the code
//! relies on a CodePointTrie generated based on some Unicode rules.
//!
//! We try to keep most of the Greek-specific logic in here, though the actual logic to remove
//! accents is in full_helper() as it must integrate with the control flow.
//!
//! This is public and doc(hidden) so that it can be accessed from the gen_greek_to_me test file,
//! and should not be used otherwise.

#[rustfmt::skip]
mod data;

pub(crate) fn get_data(ch: char) -> Option<GreekPrecomposedLetterData> {
    let ch_i = ch as usize;
    let packed = if (0x370..=0x3FF).contains(&ch_i) {
        *data::DATA_370.get(ch_i - 0x370)?
    } else if (0x1f00..0x1fff).contains(&ch_i) {
        *data::DATA_1F00.get(ch_i - 0x1f00)?
    } else {
        data::match_extras(ch)?
    };

    let packed = PackedGreekPrecomposedLetterData(packed);

    GreekPrecomposedLetterData::try_from(packed).ok()
}

/// A packed representation of [`GreekPrecomposedLetterData`]
///
/// Bit layout:
///
/// ```text
///   7       6   5   4     3   2   1       0
/// discr=0 | [diacritics]  | [vowel            ]  
/// discr=1 | [  unused = 0     ]      | [is_rho]
/// ```
///
/// Bit 7 is the discriminant. if 0, it is a vowel, else, it is a consonant.
/// If the whole thing is a zero then it is assumed to be an empty entry.
///
/// In the vowel case, the next three bits are the next three elements of [`GreekDiacritics`],
/// in order (accented, dialytika, ypogegrammeni), and the four bits after that identify
/// a [`GreekVowel`] value.
///
/// In the consonant case, the remaining seven bits identify a consonant value.
#[derive(Debug, Clone, Copy)]
pub struct PackedGreekPrecomposedLetterData(pub u8);

impl TryFrom<PackedGreekPrecomposedLetterData> for GreekPrecomposedLetterData {
    type Error = ();
    fn try_from(other: PackedGreekPrecomposedLetterData) -> Result<GreekPrecomposedLetterData, ()> {
        if other.0 == 0 {
            return Err(());
        }
        if other.0 & 0x80 == 0 {
            // vowel
            let diacritics = GreekDiacritics {
                accented: other.0 & 0x40 != 0,
                dialytika: other.0 & 0x20 != 0,
                ypogegrammeni: other.0 & 0x10 != 0,
            };
            let vowel = GreekVowel::try_from(other.0 & 0b1111);
            debug_assert!(vowel.is_ok());
            let vowel = vowel.unwrap_or(GreekVowel::Α);
            Ok(GreekPrecomposedLetterData::Vowel(vowel, diacritics))
        } else {
            // consonant
            // 0x80 is is_rho = false, 0x81 is is_rho = true
            Ok(GreekPrecomposedLetterData::Consonant(other.0 == 0x81))
        }
    }
}

impl From<GreekPrecomposedLetterData> for PackedGreekPrecomposedLetterData {
    fn from(other: GreekPrecomposedLetterData) -> Self {
        match other {
            GreekPrecomposedLetterData::Vowel(vowel, diacritics) => {
                let mut bits = 0;
                if diacritics.accented {
                    bits |= 0x40;
                }
                if diacritics.dialytika {
                    bits |= 0x20;
                }
                if diacritics.ypogegrammeni {
                    bits |= 0x10;
                }
                bits |= vowel as u8;
                PackedGreekPrecomposedLetterData(bits)
            }
            GreekPrecomposedLetterData::Consonant(is_rho) => {
                PackedGreekPrecomposedLetterData(0x80 + is_rho as u8)
            }
        }
    }
}

/// The precomposed letter data stored in the hardcoded data in `mod data`
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum GreekPrecomposedLetterData {
    /// A vowel, with a capitalized base letter, and the diacritics found
    Vowel(GreekVowel, GreekDiacritics),
    /// A consonant or vowel that does not take diacritics
    ///
    /// The boolean is true when the consonant is a rho, which is handled specially since
    /// it can take breathing marks (but is *not* a vowel)
    Consonant(bool),
}

/// n.b. these are Greek capital letters, not Latin
#[derive(Debug, Clone, Copy, PartialOrd, Ord, PartialEq, Eq)]
pub enum GreekVowel {
    // 0 is purposely left out so that the all-zero case is unambiguous
    Α = 1,
    Ε = 2,
    Η = 3,
    Ι = 4,
    Ο = 5,
    Υ = 6,
    Ω = 7,
    ϒ = 8,
}
pub const CAPITAL_RHO: char = 'Ρ';

impl From<GreekVowel> for char {
    fn from(other: GreekVowel) -> Self {
        match other {
            GreekVowel::Α => 'Α',
            GreekVowel::Ε => 'Ε',
            GreekVowel::Η => 'Η',
            GreekVowel::Ι => 'Ι',
            GreekVowel::Ο => 'Ο',
            GreekVowel::Υ => 'Υ',
            GreekVowel::Ω => 'Ω',
            GreekVowel::ϒ => 'ϒ',
        }
    }
}

impl TryFrom<char> for GreekVowel {
    type Error = ();
    fn try_from(other: char) -> Result<Self, ()> {
        Ok(match other {
            'Α' => GreekVowel::Α,
            'Ε' => GreekVowel::Ε,
            'Η' => GreekVowel::Η,
            'Ι' => GreekVowel::Ι,
            'Ο' => GreekVowel::Ο,
            'Υ' => GreekVowel::Υ,
            'Ω' => GreekVowel::Ω,
            'ϒ' => GreekVowel::ϒ,
            _ => return Err(()),
        })
    }
}

impl TryFrom<u8> for GreekVowel {
    type Error = ();
    fn try_from(other: u8) -> Result<Self, ()> {
        Ok(match other {
            1 => Self::Α,
            2 => Self::Ε,
            3 => Self::Η,
            4 => Self::Ι,
            5 => Self::Ο,
            6 => Self::Υ,
            7 => Self::Ω,
            8 => Self::ϒ,
            _ => return Err(()),
        })
    }
}

/// General diacritic information about a character or combining character sequence.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct GreekDiacritics {
    /// Whether it has an accent.
    pub accented: bool,
    /// Whether it has a dialytika.
    pub dialytika: bool,
    /// Whether it has a ypogegrammeni.
    pub ypogegrammeni: bool,
}

/// General diacritic information about a combining character sequence,
/// identifying the source of the diacritics.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct GreekCombiningCharacterSequenceDiacritics {
    // Diacritics precomposed on the base.
    pub precomposed: GreekDiacritics,
    // Combining diacritics.
    pub combining: GreekDiacritics,
}

pub const TONOS: char = '\u{0301}';
pub const DIALYTIKA: char = '\u{0308}';
pub const DIALYTIKA_TONOS: char = '\u{0344}';
pub const YPOGEGRAMMENI: char = '\u{0345}';

#[macro_export]
#[doc(hidden)] // macro
macro_rules! diacritics {
    // Accents.
    // These are mostly removed when uppercasing, but their presence may require
    // adding a διαλυτικά to a following vowel.
    (ACCENTS) => {
        // https://util.unicode.org/UnicodeJsps/list-unicodeset.jsp?a=%5B%5Cu0300+%5Cu0301+%5Cu0342+%5Cu0302+%5Cu0303+%5Cu0311%5D&g=&i=
        '\u{0300}' // Polytonic βαρεία (varia), grave accent.
        | $crate::greek_to_me::TONOS // Polytonic οξεία (oxia) unified with monotonic τόνος (tonos), acute accent.
        | '\u{0342}' // Polytonic περισπωμένη (perispomeni), often translated to circumflex.
        | '\u{0302}' // Circumflex accent, sometimes a lookalike of the περισπωμένη.
        | '\u{0303}' // Tilde, sometimes a lookalike of the περισπωμένη.
        | '\u{0311}' // Inverted breve, sometimes a lookalike of the περισπωμένη.
    };
    // Breathings and length marks.
    // These expected to occur in Greek combining sequences, and are removed when uppercasing.
    // This removal has no other effect.
    (BREATHING_AND_LENGTH) => {
        // https://util.unicode.org/UnicodeJsps/list-unicodeset.jsp?a=%5B%5Cu0304+%5Cu0306+%5Cu0313+%5Cu0314+%5Cu0343%5D&g=&i=
        '\u{0304}'  // Macron, marking long vowels.
        | '\u{0306}'  // Breve, marking short vowels.
        | '\u{0313}'  // Comma above, smooth breathing or κορωνίς marking crasis.
        | '\u{0314}'  // Reversed comma above, rough breathing.
        | '\u{0343}'  // κορωνίς (koronis), canonically decomposes to comma above.
    };
    // All diacritics containing a dialytika
    (DIALYTIKA_ALL) => { $crate::greek_to_me::DIALYTIKA | $crate::greek_to_me::DIALYTIKA_TONOS };
    (DIALYTIKA) => { $crate::greek_to_me::DIALYTIKA };
    (DIALYTIKA_TONOS) => { $crate::greek_to_me::DIALYTIKA_TONOS };
    (YPOGEGRAMMENI) => { $crate::greek_to_me::YPOGEGRAMMENI };
    ($($i:ident)|+) => { $(diacritics!($i))|+};
}

/// Macro that generates match arms for various diacritic groupings.
///
/// Groupings supported:
///
/// - `ACCENTS`
/// - `BREATHING_AND_LENGTH`
/// - `DIALYTIKA`, `DIALYTIKA_TONOS`, and `DIALITYKA_ALL`
/// - `YPOGEGRAMMENI`
///
/// This is a macro to make it easy to keep the lists of accents in sync.
pub use crate::diacritics;

impl GreekDiacritics {
    /// Whilst forwards-iterating from an existing character,
    /// consume all further greek diacritics and store their existence into this struct.
    pub(crate) fn consume_greek_diacritics(&mut self, context_after: &str) {
        for c in context_after.chars() {
            match c {
                diacritics!(ACCENTS) => self.accented = true,
                DIALYTIKA_TONOS => {
                    self.dialytika = true;
                    self.accented = true;
                }
                DIALYTIKA => self.dialytika = true,
                YPOGEGRAMMENI => self.ypogegrammeni = true,
                // Ignore other accent marks that are expected to co-occur with Greek.
                diacritics!(BREATHING_AND_LENGTH) => (),
                _ => break,
            }
        }
    }
}

/// Given the context before a character, check if it is preceded by a Greek letter.
pub(crate) fn preceded_by_greek_letter(context_before: &str) -> bool {
    for c in context_before.chars().rev() {
        match c {
            diacritics!(ACCENTS | BREATHING_AND_LENGTH | DIALYTIKA_ALL | YPOGEGRAMMENI) => continue,
            _ => return get_data(c).is_some(),
        }
    }
    false
}

/// Returns diacritic information for the combining character sequence preceding the current character
/// if it that preceding combining character sequence is a greek vowel.
pub(crate) fn preceding_greek_vowel_diacritics(
    context_before: &str,
) -> Option<GreekCombiningCharacterSequenceDiacritics> {
    let mut combining: GreekDiacritics = Default::default();
    for c in context_before.chars().rev() {
        match c {
            diacritics!(ACCENTS) => combining.accented = true,
            diacritics!(DIALYTIKA_TONOS) => {
                combining.dialytika = true;
                combining.accented = true;
            }
            diacritics!(DIALYTIKA) => combining.dialytika = true,
            diacritics!(BREATHING_AND_LENGTH) => continue,
            _ => {
                let data = get_data(c);
                if let Some(GreekPrecomposedLetterData::Vowel(_vowel, diacritics)) = data {
                    return Some(GreekCombiningCharacterSequenceDiacritics {
                        precomposed: diacritics,
                        combining,
                    });
                } else {
                    // Not a greek vowel.
                    return None;
                }
            }
        }
    }
    None
}

/// Is the character a diacritic expected to be used with greek (except ypogegrammeni).
pub(crate) fn is_greek_diacritic_except_ypogegrammeni(c: char) -> bool {
    matches!(
        c,
        diacritics!(ACCENTS | BREATHING_AND_LENGTH | DIALYTIKA_ALL)
    )
}
