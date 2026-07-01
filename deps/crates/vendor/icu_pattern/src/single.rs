// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Code for the [`SinglePlaceholder`] pattern backend.

use core::convert::Infallible;
use core::{cmp::Ordering, str::FromStr};
use writeable::adapters::WriteableAsTryWriteableInfallible;
use writeable::Writeable;

use crate::common::*;
use crate::Error;

#[cfg(feature = "alloc")]
use alloc::{boxed::Box, string::String};

/// A singleton enum for the [`SinglePlaceholder`] pattern backend.
///
/// # Examples
///
/// ```
/// use core::cmp::Ordering;
/// use core::str::FromStr;
/// use icu_pattern::PatternItem;
/// use icu_pattern::SinglePlaceholder;
/// use icu_pattern::SinglePlaceholderKey;
/// use icu_pattern::SinglePlaceholderPattern;
///
/// // Parse the string syntax and check the resulting data store:
/// let pattern = SinglePlaceholderPattern::try_from_str(
///     "Hello, {0}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// assert_eq!(
///     pattern.iter().cmp(
///         [
///             PatternItem::Literal("Hello, "),
///             PatternItem::Placeholder(SinglePlaceholderKey::Singleton),
///             PatternItem::Literal("!")
///         ]
///         .into_iter()
///     ),
///     Ordering::Equal
/// );
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize, serde::Serialize))]
#[allow(clippy::exhaustive_enums)] // Singleton
pub enum SinglePlaceholderKey {
    Singleton,
}

impl FromStr for SinglePlaceholderKey {
    type Err = Infallible;
    fn from_str(_: &str) -> Result<Self, Self::Err> {
        Ok(Self::Singleton)
    }
}

impl<W> PlaceholderValueProvider<SinglePlaceholderKey> for (W,)
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

    fn value_for(&self, _key: SinglePlaceholderKey) -> Self::W<'_> {
        WriteableAsTryWriteableInfallible(&self.0)
    }
    #[inline]
    fn map_literal<'a, 'l>(&'a self, literal: &'l str) -> Self::L<'a, 'l> {
        literal
    }
}

impl<W> PlaceholderValueProvider<SinglePlaceholderKey> for [W; 1]
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

    fn value_for(&self, _key: SinglePlaceholderKey) -> Self::W<'_> {
        let [value] = self;
        WriteableAsTryWriteableInfallible(value)
    }
    #[inline]
    fn map_literal<'a, 'l>(&'a self, literal: &'l str) -> Self::L<'a, 'l> {
        literal
    }
}

/// Backend for patterns containing zero or one placeholder.
///
/// This empty type is not constructible.
///
/// # Placeholder Keys
///
/// The placeholder is always [`SinglePlaceholderKey::Singleton`].
///
/// In [`Pattern::interpolate()`], pass a single-element array or tuple.
///
/// # Encoding Details
///
/// The first code point of the string is 1 plus the byte offset of the placeholder counting from
/// after that initial code point. If zero, there is no placeholder.
///
/// # Examples
///
/// Parsing a pattern into the encoding:
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::Pattern;
/// use icu_pattern::SinglePlaceholder;
///
/// // Parse the string syntax and check the resulting data store:
/// let pattern = Pattern::<SinglePlaceholder>::try_from_str(
///     "Hello, {0}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// assert_eq!("\u{8}Hello, !", &pattern.store);
/// ```
///
/// Example patterns supported by this backend:
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::Pattern;
/// use icu_pattern::QuoteMode;
/// use icu_pattern::SinglePlaceholder;
///
/// // Single numeric placeholder:
/// assert_eq!(
///     Pattern::<SinglePlaceholder>::try_from_str(
///         "{0} days ago",
///         Default::default()
///     )
///     .unwrap()
///     .interpolate_to_string([5]),
///     "5 days ago",
/// );
///
/// // Single named placeholder:
/// assert_eq!(
///     Pattern::<SinglePlaceholder>::try_from_str(
///         "{name}",
///         Default::default()
///     )
///     .unwrap()
///     .interpolate_to_string(["Alice"]),
///     "Alice",
/// );
///
/// // No placeholder (note, the placeholder value is never accessed):
/// assert_eq!(
///     Pattern::<SinglePlaceholder>::try_from_str(
///         "yesterday",
///         Default::default()
///     )
///     .unwrap()
///     .interpolate_to_string(["hi"]),
///     "yesterday",
/// );
///
/// // Escaped placeholder and a real placeholder:
/// assert_eq!(
///     Pattern::<SinglePlaceholder>::try_from_str(
///         "'{0}' {1}",
///         QuoteMode::QuotingSupported.into()
///     )
///     .unwrap()
///     .interpolate_to_string(("hi",)),
///     "{0} hi",
/// );
/// ```
///
/// [`Pattern::interpolate()`]: crate::Pattern::interpolate
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[allow(clippy::exhaustive_enums)] // Empty Enum
pub enum SinglePlaceholder {}

impl crate::private::Sealed for SinglePlaceholder {}

impl PatternBackend for SinglePlaceholder {
    type PlaceholderKey<'a> = SinglePlaceholderKey;
    #[cfg(feature = "alloc")]
    type PlaceholderKeyCow<'a> = SinglePlaceholderKey;
    type Error<'a> = Infallible;
    type Store = str;
    type Iter<'a> = SinglePlaceholderPatternIterator<'a>;

    fn validate_store(store: &Self::Store) -> Result<(), Error> {
        let placeholder_offset_char = store.chars().next().ok_or(Error::InvalidPattern)?;
        let initial_offset = placeholder_offset_char.len_utf8();
        let placeholder_offset = placeholder_offset_char as usize;
        if placeholder_offset > store.len() - initial_offset + 1 {
            return Err(Error::InvalidPattern);
        }
        if placeholder_offset >= 0xD800 {
            return Err(Error::InvalidPattern);
        }
        Ok(())
    }

    fn iter_items(store: &Self::Store) -> Self::Iter<'_> {
        let placeholder_offset_char = match store.chars().next() {
            Some(i) => i,
            None => {
                debug_assert!(false);
                '\0'
            }
        };
        let initial_offset = placeholder_offset_char.len_utf8();
        SinglePlaceholderPatternIterator {
            store,
            placeholder_offset: placeholder_offset_char as usize + initial_offset - 1,
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
        let mut seen_placeholder = false;
        for item in items {
            match item? {
                PatternItemCow::Literal(s) => result.push_str(&s),
                PatternItemCow::Placeholder(_) if !seen_placeholder => {
                    seen_placeholder = true;
                    let placeholder_offset =
                        u32::try_from(result.len() + 1).map_err(|_| Error::InvalidPattern)?;
                    if placeholder_offset >= 0xD800 {
                        return Err(Error::InvalidPattern);
                    }
                    let placeholder_offset_char =
                        char::try_from(placeholder_offset).map_err(|_| Error::InvalidPattern)?;
                    result.insert(0, placeholder_offset_char);
                }
                PatternItemCow::Placeholder(_) => {
                    return Err(Error::InvalidPattern);
                }
            }
        }
        if !seen_placeholder {
            result.insert(0, '\0');
        }
        Ok(result.into_boxed_str())
    }

    fn empty() -> &'static Self::Store {
        "\0"
    }
}

#[doc(hidden)] // TODO(#4467): Should be internal
#[derive(Debug)]
pub struct SinglePlaceholderPatternIterator<'a> {
    store: &'a str,
    placeholder_offset: usize,
    current_offset: usize,
}

// Note: This impl is not exported via public bounds, but it might be in the
// future, and the compiler might be able to find it. The code is also
// reachable from `Iterator::size_hint`.
impl ExactSizeIterator for SinglePlaceholderPatternIterator<'_> {
    fn len(&self) -> usize {
        let placeholder_offset_char = match self.store.chars().next() {
            Some(i) => i,
            None => {
                debug_assert!(false);
                '\0'
            }
        };
        let initial_offset = placeholder_offset_char.len_utf8();
        let placeholder_offset = placeholder_offset_char as usize + initial_offset - 1;
        let store_len = self.store.len();
        if placeholder_offset < initial_offset {
            // No placeholder
            if initial_offset < store_len {
                // No placeholder, non-empty literal
                1
            } else {
                // No placeholder, empty literal
                0
            }
        } else if placeholder_offset == initial_offset {
            // Has placeholder, empty prefix
            if initial_offset < store_len {
                // Has placeholder, empty prefix, non-empty suffix
                2
            } else {
                // Has placeholder, empty prefix, empty suffix
                1
            }
        } else if placeholder_offset < store_len {
            // Has placeholder, non-empty prefix, non-empty suffix
            3
        } else {
            // Has placeholder, non-empty prefix, empty suffix
            2
        }
    }
}

impl<'a> Iterator for SinglePlaceholderPatternIterator<'a> {
    type Item = PatternItem<'a, SinglePlaceholderKey>;
    fn next(&mut self) -> Option<Self::Item> {
        match self.current_offset.cmp(&self.placeholder_offset) {
            Ordering::Less => {
                // Prefix
                let literal_str = match self.store.get(self.current_offset..self.placeholder_offset)
                {
                    Some(s) => s,
                    None => {
                        debug_assert!(false, "offsets are in range");
                        ""
                    }
                };
                self.current_offset = self.placeholder_offset;
                Some(PatternItem::Literal(literal_str))
            }
            Ordering::Equal => {
                // Placeholder
                self.placeholder_offset = 0;
                Some(PatternItem::Placeholder(SinglePlaceholderKey::Singleton))
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
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.len();
        (len, Some(len))
    }
}

// TODO(#1668):  Add more tests
