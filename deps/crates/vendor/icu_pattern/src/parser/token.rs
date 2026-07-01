// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::borrow::Cow;

/// A [`PatternItem`] with additional detail returned by the [`Parser`].
///
/// ✨ *Enabled with the `alloc` Cargo feature.*
///
/// # Examples
///
/// ```
/// use icu_pattern::{ParsedPatternItem, Parser, ParserOptions};
///
/// let input = "{0}, {1}";
///
/// let mut parser = Parser::new(input, ParserOptions::default());
///
/// let mut result = vec![];
///
/// while let Some(element) =
///     parser.try_next().expect("Failed to advance iterator")
/// {
///     result.push(element);
/// }
///
/// assert_eq!(
///     result,
///     &[
///         ParsedPatternItem::Placeholder(0),
///         ParsedPatternItem::Literal {
///             content: ", ".into(),
///             quoted: false
///         },
///         ParsedPatternItem::Placeholder(1),
///     ]
/// );
/// ```
///
/// # Type parameters
///
/// - `P`: A placeholder type which implements [`FromStr`].
///
/// # Lifetimes
///
/// - `s`: The life time of an input string slice being parsed.
///
/// [`Parser`]: crate::Parser
/// [`PatternItem`]: crate::PatternItem
/// [`FromStr`]: core::str::FromStr
#[derive(PartialEq, Debug, Clone)]
#[non_exhaustive]
pub enum ParsedPatternItem<'s, P> {
    Placeholder(P),
    Literal { content: Cow<'s, str>, quoted: bool },
}

impl<'s, P> From<(&'s str, bool)> for ParsedPatternItem<'s, P> {
    fn from(input: (&'s str, bool)) -> Self {
        Self::Literal {
            content: Cow::Borrowed(input.0),
            quoted: input.1,
        }
    }
}
