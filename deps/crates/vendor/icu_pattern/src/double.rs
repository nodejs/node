// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Code for the [`DoublePlaceholder`] pattern backend.

use core::convert::Infallible;
use core::{cmp::Ordering, str::FromStr};
use either::Either;
use writeable::adapters::WriteableAsTryWriteableInfallible;
use writeable::Writeable;

use crate::common::*;
use crate::Error;

#[cfg(feature = "alloc")]
use alloc::{boxed::Box, string::String};

/// A two-value enum for the [`DoublePlaceholder`] pattern backend.
///
/// # Examples
///
/// ```
/// use core::cmp::Ordering;
/// use core::str::FromStr;
/// use icu_pattern::DoublePlaceholderKey;
/// use icu_pattern::DoublePlaceholderPattern;
/// use icu_pattern::PatternItem;
///
/// // Parse the string syntax
/// let pattern = DoublePlaceholderPattern::try_from_str(
///     "Hello, {0} and {1}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// assert_eq!(
///     pattern.iter().cmp(
///         [
///             PatternItem::Literal("Hello, "),
///             PatternItem::Placeholder(DoublePlaceholderKey::Place0),
///             PatternItem::Literal(" and "),
///             PatternItem::Placeholder(DoublePlaceholderKey::Place1),
///             PatternItem::Literal("!")
///         ]
///         .into_iter()
///     ),
///     Ordering::Equal
/// );
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize, serde::Serialize))]
#[allow(clippy::exhaustive_enums)] // Defined to have two entries
pub enum DoublePlaceholderKey {
    /// The placeholder `{0}`.
    Place0,
    /// The placeholder `{1}`.
    Place1,
}

impl FromStr for DoublePlaceholderKey {
    type Err = Error;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "0" => Ok(Self::Place0),
            "1" => Ok(Self::Place1),
            _ => Err(Error::InvalidPlaceholder),
        }
    }
}

impl<W0, W1> PlaceholderValueProvider<DoublePlaceholderKey> for (W0, W1)
where
    W0: Writeable,
    W1: Writeable,
{
    type Error = Infallible;

    type W<'a>
        = WriteableAsTryWriteableInfallible<Either<&'a W0, &'a W1>>
    where
        Self: 'a;

    type L<'a, 'l>
        = &'l str
    where
        Self: 'a;

    #[inline]
    fn value_for(&self, key: DoublePlaceholderKey) -> Self::W<'_> {
        let writeable = match key {
            DoublePlaceholderKey::Place0 => Either::Left(&self.0),
            DoublePlaceholderKey::Place1 => Either::Right(&self.1),
        };
        WriteableAsTryWriteableInfallible(writeable)
    }
    #[inline]
    fn map_literal<'a, 'l>(&'a self, literal: &'l str) -> Self::L<'a, 'l> {
        literal
    }
}

impl<W> PlaceholderValueProvider<DoublePlaceholderKey> for [W; 2]
where
    W: Writeable,
{
    type Error = Infallible;

    type W<'a>
        = WriteableAsTryWriteableInfallible<&'a W>
    where
        Self: 'a;

    type L<'a, 'l>
        = &'l str
    where
        Self: 'a;

    #[inline]
    fn value_for(&self, key: DoublePlaceholderKey) -> Self::W<'_> {
        let [item0, item1] = self;
        let writeable = match key {
            DoublePlaceholderKey::Place0 => item0,
            DoublePlaceholderKey::Place1 => item1,
        };
        WriteableAsTryWriteableInfallible(writeable)
    }
    #[inline]
    fn map_literal<'a, 'l>(&'a self, literal: &'l str) -> Self::L<'a, 'l> {
        literal
    }
}

/// Internal representation of a placeholder
#[derive(Debug, Copy, Clone)]
struct DoublePlaceholderInfo {
    /// The placeholder key: 0 or 1
    pub key: DoublePlaceholderKey,
    /// An offset field. This can take one of two forms:
    /// - Encoded form: 1 + offset from start of literals
    /// - Decoded form: offset from start of store
    pub offset: usize,
}

impl DoublePlaceholderInfo {
    pub fn from_char(ch: char) -> Self {
        Self {
            key: if ((ch as usize) & 0x1) == 0 {
                DoublePlaceholderKey::Place0
            } else {
                DoublePlaceholderKey::Place1
            },
            offset: (ch as usize) >> 1,
        }
    }
    #[cfg(feature = "alloc")]
    pub fn try_to_char(self) -> Result<char, Error> {
        let usize_val = match self.key {
            DoublePlaceholderKey::Place0 => 0,
            DoublePlaceholderKey::Place1 => 1,
        } | (self.offset << 1);
        u32::try_from(usize_val)
            .ok()
            .and_then(|x| char::try_from(x).ok())
            .ok_or(Error::InvalidPattern)
    }
    /// Creates a [`DoublePlaceholderInfo`] for an empty `Place0`
    pub fn no_place0() -> Self {
        Self {
            key: DoublePlaceholderKey::Place0,
            offset: 0,
        }
    }
    /// Changes Place0 to Place1 and vice-versa
    pub fn swap(self) -> Self {
        Self {
            key: match self.key {
                DoublePlaceholderKey::Place0 => DoublePlaceholderKey::Place1,
                DoublePlaceholderKey::Place1 => DoublePlaceholderKey::Place0,
            },
            offset: self.offset,
        }
    }
    /// Sets the offset to 0 (ignored placeholder), retaining the key
    #[cfg(feature = "alloc")]
    pub fn clear(self) -> Self {
        Self {
            key: self.key,
            offset: 0,
        }
    }
}

/// Backend for patterns containing zero, one, or two placeholders.
///
/// This empty type is not constructible.
///
/// # Placeholder Keys
///
/// The placeholder is either [`DoublePlaceholderKey::Place0`] or [`DoublePlaceholderKey::Place1`].
///
/// In [`Pattern::interpolate()`], pass a two-element array or tuple.
///
/// # Encoding Details
///
/// The first two code points contain the indices of the first and second placeholders with
/// the following encoding:
///
/// 1. First bit: 0 for [`DoublePlaceholderKey::Place0`], 1 for [`DoublePlaceholderKey::Place1`].
/// 2. Second and higher bits: 1 plus the byte offset of the placeholder counting from after
///    the placeholder index code points. If zero, skip this placeholder.
///
/// # Examples
///
/// Parsing a pattern into the encoding:
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::DoublePlaceholder;
/// use icu_pattern::Pattern;
///
/// // Parse the string syntax and check the resulting data store:
/// let pattern = Pattern::<DoublePlaceholder>::try_from_str(
///     "Hello, {0} and {1}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// assert_eq!("\x10\x1BHello,  and !", &pattern.store);
/// ```
///
/// Explanation of the lead code points:
///
/// 1. `\x10` is placeholder 0 at index 7: ((7 + 1) << 1) | 0
/// 2. `\x1B` is placeholder 1 at index 12: ((12 + 1) << 1) | 1
///
/// Example patterns supported by this backend:
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::DoublePlaceholder;
/// use icu_pattern::Pattern;
/// use icu_pattern::QuoteMode;
///
/// // Single numeric placeholder (note, "5" is used):
/// assert_eq!(
///     Pattern::<DoublePlaceholder>::try_from_str(
///         "{0} days ago",
///         Default::default()
///     )
///     .unwrap()
///     .interpolate_to_string([5, 7]),
///     "5 days ago",
/// );
///
/// // No placeholder (note, the placeholder value is never accessed):
/// assert_eq!(
///     Pattern::<DoublePlaceholder>::try_from_str(
///         "yesterday",
///         Default::default()
///     )
///     .unwrap()
///     .interpolate_to_string(["foo", "bar"]),
///     "yesterday",
/// );
///
/// // Escaped placeholder and a placeholder value 1 (note, "bar" is used):
/// assert_eq!(
///     Pattern::<DoublePlaceholder>::try_from_str(
///         "'{0}' {1}",
///         QuoteMode::QuotingSupported.into()
///     )
///     .unwrap()
///     .interpolate_to_string(("foo", "bar")),
///     "{0} bar",
/// );
///
/// // Pattern with the placeholders in the opposite order:
/// assert_eq!(
///     Pattern::<DoublePlaceholder>::try_from_str(
///         "A {1} B {0} C",
///         Default::default()
///     )
///     .unwrap()
///     .interpolate_to_string(("D", "E")),
///     "A E B D C",
/// );
///
/// // No literals, only placeholders:
/// assert_eq!(
///     Pattern::<DoublePlaceholder>::try_from_str(
///         "{1}{0}",
///         Default::default()
///     )
///     .unwrap()
///     .interpolate_to_string((1, "A")),
///     "A1",
/// );
/// ```
///
/// [`Pattern::interpolate()`]: crate::Pattern::interpolate
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[allow(clippy::exhaustive_enums)] // Empty Enum
pub enum DoublePlaceholder {}

impl crate::private::Sealed for DoublePlaceholder {}

impl PatternBackend for DoublePlaceholder {
    type PlaceholderKey<'a> = DoublePlaceholderKey;
    #[cfg(feature = "alloc")]
    type PlaceholderKeyCow<'a> = DoublePlaceholderKey;
    type Error<'a> = Infallible;
    type Store = str;
    type Iter<'a> = DoublePlaceholderPatternIterator<'a>;

    fn validate_store(store: &Self::Store) -> Result<(), Error> {
        let mut chars = store.chars();
        let ph_first_char = chars.next().ok_or(Error::InvalidPattern)?;
        let ph_second_char = chars.next().ok_or(Error::InvalidPattern)?;
        let initial_offset = ph_first_char.len_utf8() + ph_second_char.len_utf8();
        let ph_first = DoublePlaceholderInfo::from_char(ph_first_char);
        let ph_second = DoublePlaceholderInfo::from_char(ph_second_char);
        if ph_first.key == ph_second.key {
            return Err(Error::InvalidPattern);
        }
        if ph_first.offset > ph_second.offset && ph_second.offset > 0 {
            return Err(Error::InvalidPattern);
        }
        if ph_second.offset > store.len() - initial_offset + 1 {
            return Err(Error::InvalidPattern);
        }
        if (ph_second_char as usize) >= 0xD800 {
            return Err(Error::InvalidPattern);
        }
        Ok(())
    }

    fn iter_items(store: &Self::Store) -> Self::Iter<'_> {
        let mut chars = store.chars();
        let (mut ph_first, ph_first_len) = match chars.next() {
            Some(ch) => (DoublePlaceholderInfo::from_char(ch), ch.len_utf8()),
            None => {
                debug_assert!(false);
                (DoublePlaceholderInfo::no_place0(), 0)
            }
        };
        let (mut ph_second, ph_second_len) = match chars.next() {
            Some(ch) => (DoublePlaceholderInfo::from_char(ch), ch.len_utf8()),
            None => {
                debug_assert!(false);
                (ph_first.swap(), ph_first_len)
            }
        };
        let initial_offset = ph_first_len + ph_second_len;
        ph_first.offset += initial_offset - 1;
        ph_second.offset += initial_offset - 1;
        DoublePlaceholderPatternIterator {
            store,
            ph_first,
            ph_second,
            current_offset: initial_offset,
        }
    }

    #[cfg(feature = "alloc")]
    fn try_from_items<
        'cow,
        'ph,
        I: Iterator<Item = Result<PatternItemCow<'cow, Self::PlaceholderKey<'ph>>, Error>>,
    >(
        items: I,
    ) -> Result<Box<str>, Error> {
        let mut result = String::new();
        let mut first_ph = None;
        let mut second_ph = None;
        for item in items {
            match item? {
                PatternItemCow::Literal(s) => result.push_str(&s),
                PatternItemCow::Placeholder(ph_key) => {
                    if second_ph.is_some() {
                        return Err(Error::InvalidPattern);
                    }
                    let placeholder_offset = result.len() + 1;
                    if placeholder_offset >= 0xD800 {
                        return Err(Error::InvalidPattern);
                    }
                    let ph_info = DoublePlaceholderInfo {
                        key: ph_key,
                        offset: placeholder_offset,
                    };
                    if first_ph.is_none() {
                        first_ph.replace(ph_info);
                    } else {
                        second_ph.replace(ph_info);
                    }
                }
            }
        }
        let (first_ph, second_ph) = match (first_ph, second_ph) {
            (Some(a), Some(b)) => (a, b),
            (Some(a), None) => (a, a.swap().clear()),
            (None, None) => (
                DoublePlaceholderInfo::no_place0(),
                DoublePlaceholderInfo::no_place0().swap(),
            ),
            (None, Some(_)) => unreachable!("first_ph always populated before second_ph"),
        };
        if first_ph.key == second_ph.key {
            return Err(Error::InvalidPattern);
        }

        result.insert(0, second_ph.try_to_char()?);
        result.insert(0, first_ph.try_to_char()?);

        Ok(result.into_boxed_str())
    }

    fn empty() -> &'static Self::Store {
        "\0\u{1}"
    }
}

#[doc(hidden)] // TODO(#4467): Should be internal
#[derive(Debug)]
pub struct DoublePlaceholderPatternIterator<'a> {
    store: &'a str,
    ph_first: DoublePlaceholderInfo,
    ph_second: DoublePlaceholderInfo,
    current_offset: usize,
}

// Note: This impl is not exported via public bounds, but it might be in the
// future, and the compiler might be able to find it. The code is also
// reachable from `Iterator::size_hint`.
impl ExactSizeIterator for DoublePlaceholderPatternIterator<'_> {
    fn len(&self) -> usize {
        let mut chars = self.store.chars();
        let (mut ph_first, ph_first_len) = match chars.next() {
            Some(ch) => (DoublePlaceholderInfo::from_char(ch), ch.len_utf8()),
            None => {
                debug_assert!(false);
                (DoublePlaceholderInfo::no_place0(), 0)
            }
        };
        let (mut ph_second, ph_second_len) = match chars.next() {
            Some(ch) => (DoublePlaceholderInfo::from_char(ch), ch.len_utf8()),
            None => {
                debug_assert!(false);
                (ph_first.swap(), ph_first_len)
            }
        };
        let initial_offset = ph_first_len + ph_second_len;
        ph_first.offset += initial_offset - 1;
        ph_second.offset += initial_offset - 1;
        let store_len = self.store.len();

        #[expect(clippy::comparison_chain)]
        if ph_first.offset < initial_offset {
            // No placeholders
            if initial_offset < store_len {
                // No placeholder, non-empty literal
                1
            } else {
                // No placeholder, empty literal
                0
            }
        } else if ph_first.offset == initial_offset {
            // At least 1 placeholder, empty prefix
            if ph_second.offset < initial_offset {
                // Single placeholder
                if ph_first.offset < store_len {
                    // Single placeholder, empty prefix, non-empty suffix
                    2
                } else {
                    // Single placeholder, empty prefix, empty suffix
                    1
                }
            } else if ph_second.offset == ph_first.offset {
                // Two placeholders, empty infix
                if ph_first.offset < store_len {
                    // Two placeholders, empty prefix, empty infix, non-empty suffix
                    3
                } else {
                    // Two placeholders, empty prefix, empty infix, empty suffix
                    2
                }
            } else if ph_second.offset < store_len {
                // Two placeholders, empty prefix, non-empty infix, non-empty suffix
                4
            } else {
                // Two placeholders, empty prefix, non-empty infix, empty suffix
                3
            }
        } else {
            // At least 1 placeholder, non-empty prefix
            if ph_second.offset < initial_offset {
                // Single placeholder
                if ph_first.offset < store_len {
                    // Single placeholder, non-empty prefix, non-empty suffix
                    3
                } else {
                    // Single placeholder, non-empty prefix, empty suffix
                    2
                }
            } else if ph_second.offset == ph_first.offset {
                // Two placeholders, empty infix
                if ph_first.offset < store_len {
                    // Two placeholders, non-empty prefix, empty infix, non-empty suffix
                    4
                } else {
                    // Two placeholders, non-empty prefix, empty infix, empty suffix
                    3
                }
            } else if ph_second.offset < store_len {
                // Two placeholders, non-empty prefix, non-empty infix, non-empty suffix
                5
            } else {
                // Two placeholders, non-empty prefix, non-empty infix, empty suffix
                4
            }
        }
    }
}

impl<'a> Iterator for DoublePlaceholderPatternIterator<'a> {
    type Item = PatternItem<'a, DoublePlaceholderKey>;
    fn next(&mut self) -> Option<Self::Item> {
        match self.current_offset.cmp(&self.ph_first.offset) {
            Ordering::Less => {
                // Prefix
                let literal_str = match self.store.get(self.current_offset..self.ph_first.offset) {
                    Some(s) => s,
                    None => {
                        debug_assert!(false, "offsets are in range");
                        ""
                    }
                };
                self.current_offset = self.ph_first.offset;
                Some(PatternItem::Literal(literal_str))
            }
            Ordering::Equal => {
                // Placeholder
                self.ph_first.offset = 0;
                Some(PatternItem::Placeholder(self.ph_first.key))
            }
            Ordering::Greater => match self.current_offset.cmp(&self.ph_second.offset) {
                Ordering::Less => {
                    // Infix
                    let literal_str =
                        match self.store.get(self.current_offset..self.ph_second.offset) {
                            Some(s) => s,
                            None => {
                                debug_assert!(false, "offsets are in range");
                                ""
                            }
                        };
                    self.current_offset = self.ph_second.offset;
                    Some(PatternItem::Literal(literal_str))
                }
                Ordering::Equal => {
                    // Placeholder
                    self.ph_second.offset = 0;
                    Some(PatternItem::Placeholder(self.ph_second.key))
                }
                Ordering::Greater => {
                    // Suffix or end of string
                    let literal_str = match self.store.get(self.current_offset..) {
                        Some(s) => s,
                        None => {
                            debug_assert!(false, "offsets are in range");
                            ""
                        }
                    };
                    if literal_str.is_empty() {
                        // End of string
                        None
                    } else {
                        // Suffix
                        self.current_offset = self.store.len();
                        Some(PatternItem::Literal(literal_str))
                    }
                }
            },
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.len();
        (len, Some(len))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::DoublePlaceholderPattern;

    #[test]
    fn test_permutations() {
        #[derive(Debug)]
        struct TestCase<'a> {
            pattern: &'a str,
            store: &'a str,
            /// Interpolation with 0=apple, 1=orange
            interpolated: &'a str,
        }
        let cases = [
            TestCase {
                pattern: "",
                store: "\x00\x01",
                interpolated: "",
            },
            TestCase {
                pattern: "{0}",
                store: "\x02\x01",
                interpolated: "apple",
            },
            TestCase {
                pattern: "X{0}",
                store: "\x04\x01X",
                interpolated: "Xapple",
            },
            TestCase {
                pattern: "{0}Y",
                store: "\x02\x01Y",
                interpolated: "appleY",
            },
            TestCase {
                pattern: "X{0}Y",
                store: "\x04\x01XY",
                interpolated: "XappleY",
            },
            TestCase {
                pattern: "{1}",
                store: "\x03\x00",
                interpolated: "orange",
            },
            TestCase {
                pattern: "X{1}",
                store: "\x05\x00X",
                interpolated: "Xorange",
            },
            TestCase {
                pattern: "{1}Y",
                store: "\x03\x00Y",
                interpolated: "orangeY",
            },
            TestCase {
                pattern: "X{1}Y",
                store: "\x05\x00XY",
                interpolated: "XorangeY",
            },
            TestCase {
                pattern: "{0}{1}",
                store: "\x02\x03",
                interpolated: "appleorange",
            },
            TestCase {
                pattern: "X{0}{1}",
                store: "\x04\x05X",
                interpolated: "Xappleorange",
            },
            TestCase {
                pattern: "{0}Y{1}",
                store: "\x02\x05Y",
                interpolated: "appleYorange",
            },
            TestCase {
                pattern: "{0}{1}Z",
                store: "\x02\x03Z",
                interpolated: "appleorangeZ",
            },
            TestCase {
                pattern: "X{0}Y{1}",
                store: "\x04\x07XY",
                interpolated: "XappleYorange",
            },
            TestCase {
                pattern: "X{0}{1}Z",
                store: "\x04\x05XZ",
                interpolated: "XappleorangeZ",
            },
            TestCase {
                pattern: "{0}Y{1}Z",
                store: "\x02\x05YZ",
                interpolated: "appleYorangeZ",
            },
            TestCase {
                pattern: "X{0}Y{1}Z",
                store: "\x04\x07XYZ",
                interpolated: "XappleYorangeZ",
            },
            TestCase {
                pattern: "{1}{0}",
                store: "\x03\x02",
                interpolated: "orangeapple",
            },
            TestCase {
                pattern: "X{1}{0}",
                store: "\x05\x04X",
                interpolated: "Xorangeapple",
            },
            TestCase {
                pattern: "{1}Y{0}",
                store: "\x03\x04Y",
                interpolated: "orangeYapple",
            },
            TestCase {
                pattern: "{1}{0}Z",
                store: "\x03\x02Z",
                interpolated: "orangeappleZ",
            },
            TestCase {
                pattern: "X{1}Y{0}",
                store: "\x05\x06XY",
                interpolated: "XorangeYapple",
            },
            TestCase {
                pattern: "X{1}{0}Z",
                store: "\x05\x04XZ",
                interpolated: "XorangeappleZ",
            },
            TestCase {
                pattern: "{1}Y{0}Z",
                store: "\x03\x04YZ",
                interpolated: "orangeYappleZ",
            },
            TestCase {
                pattern: "X{1}Y{0}Z",
                store: "\x05\x06XYZ",
                interpolated: "XorangeYappleZ",
            },
            TestCase {
                pattern: "01234567890123456789012345678901234567890123456789012345678901234567890123456789{0}Y{1}Z",
                store: "\u{A2}\u{A5}01234567890123456789012345678901234567890123456789012345678901234567890123456789YZ",
                interpolated: "01234567890123456789012345678901234567890123456789012345678901234567890123456789appleYorangeZ",
            },
        ];
        for cas in cases {
            let TestCase {
                pattern,
                store,
                interpolated,
            } = cas;
            let parsed =
                DoublePlaceholderPattern::try_from_str(pattern, Default::default()).unwrap();
            let actual = parsed
                .interpolate(["apple", "orange"])
                .write_to_string()
                .into_owned();
            assert_eq!(&parsed.store, store, "{cas:?}");
            assert_eq!(actual, interpolated, "{cas:?}");
        }
    }

    #[test]
    fn test_invalid() {
        let cases = [
            "",               // too short
            "\x00",           // too short
            "\x00\x00",       // duplicate placeholders
            "\x04\x03",       // first offset is after second offset
            "\x04\x05",       // second offset out of range (also first offset)
            "\x04\u{10001}@", // second offset too large for UTF-8
        ];
        let long_str = "0123456789".repeat(1000000);
        for cas in cases {
            let cas = cas.replace('@', &long_str);
            assert!(DoublePlaceholder::validate_store(&cas).is_err(), "{cas:?}");
        }
    }
}
