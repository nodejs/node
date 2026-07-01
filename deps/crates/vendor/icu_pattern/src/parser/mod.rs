// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

pub mod error;
pub mod token;

use alloc::{borrow::Cow, vec, vec::Vec};
use core::{fmt::Debug, marker::PhantomData, str::FromStr};
pub use error::ParserError;
pub use token::ParsedPatternItem;

#[derive(PartialEq, Debug, Default)]
enum ParserState {
    #[default]
    Default,
    Placeholder,
    QuotedLiteral,
    Apostrophe {
        quoted: bool,
    },
}

macro_rules! handle_literal {
    ($self:ident, $quoted:expr, $next_state:expr) => {{
        let range = $self.advance_state($self.idx, $next_state);
        if !range.is_empty() {
            return Ok(Some(ParsedPatternItem::Literal {
                content: Cow::Borrowed(&$self.input[range]),
                quoted: $quoted,
            }));
        } else {
            continue;
        }
    }};
}

/// Options passed to the constructor of [`Parser`].
///
/// ✨ *Enabled with the `alloc` Cargo feature.*
#[derive(Debug, Default)]
#[non_exhaustive]
pub struct ParserOptions {
    /// Controls how quotes (`'`) are interpreted.
    pub quote_mode: QuoteMode,
}

/// Controls how quotes (`'`) are interpreted.
#[derive(Debug, Default, PartialEq)]
#[non_exhaustive]
pub enum QuoteMode {
    /// Quotes are interpreted as literals, i.e. `{0} o'clock` will interpolate to `5 o'clock`.
    #[default]
    QuotesAreLiterals,
    /// Quotes can be used to quote ASCII characters, i.e. both `{0} World` and `{0} 'World'` will interpolate to `Hello World`.
    ///
    /// A double quote can be used to create a quote literal, i.e. `{0} o''clock`.
    QuotingSupported,
    /// Quotes are required to quote ASCII characters, i.e. `{0} 'World'` will interpolate to `Hello World`, while `{0} World` is an error.
    ///
    /// A double quote can be used to create a quote literal, i.e. `{0} 'o''clock'`.
    QuotingRequired,
}

impl From<QuoteMode> for ParserOptions {
    fn from(quote_mode: QuoteMode) -> Self {
        Self { quote_mode }
    }
}

/// Placeholder pattern parser.
///
/// The parser allows for handling flexible range of generic patterns
/// with placeholders.
///
/// The [`Parser`] is generic over any placeholder which implements [`FromStr`]
/// allowing the consumer to parse placeholder patterns such as "{0}, {1}",
/// "{date}, {time}" or any other. A placeholder must be enclosed in `{` and `}`
/// characters in the input pattern string.
///
/// At the moment the parser is written as a custom fallible iterator.
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
/// # Named placeholders
///
/// The parser is also capable of parsing different placeholder types such as strings.
///
/// ## Examples
/// ```
/// use icu_pattern::{ParsedPatternItem, Parser, ParserOptions};
///
/// let input = "{start}, {end}";
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
///         ParsedPatternItem::Placeholder("start".to_owned()),
///         ParsedPatternItem::Literal {
///             content: ", ".into(),
///             quoted: false
///         },
///         ParsedPatternItem::Placeholder("end".to_owned()),
///     ]
/// );
/// ```
///
/// # Type parameters
///
/// - `P`: The type of the placeholder used as a key for the [`PlaceholderValueProvider`].
///
/// # Lifetimes
///
/// - `p`: The life time of an input string slice to be parsed.
///
/// # Design Decisions
///
/// The parser is written in an intentionally generic way to enable use against wide range
/// of potential placeholder pattern models and use cases.
///
/// Serveral design decisions have been made that the reader should be aware of when using the API.
///
/// ## Zero copy
///
/// The parser is intended for runtime use and is optimized for performance and low memory overhad.
///
/// Zero copy parsing is a model which allows the parser to produce tokens that are de-facto
/// slices of the input without ever having to modify the input or copy from it.
///
/// In case of ICU patterns that decision brings a trade-off around handling of quoted literals.
/// A parser that copies bytes from the input when generating the output can take a pattern literal
/// that contains a quoted portion and concatenace the parts, effectively generating a single
/// literal out of a series of syntactical literal quoted and unquoted nodes.
/// A zero copy parser sacrifices that convenience for marginal performance gains.
///
/// The rationale for the decision is that many placeholder patterns do not contain ASCII letters
/// and therefore can benefit from this design decision.
/// Secondly, even in scenarios where ASCII letters, or other quoted literals, are used, the
/// zero-copy design still maintains high performance, only increasing the number of tokens
/// returned by the parser, but without increase to allocations.
///
/// ### Examples
/// ```
/// use icu_pattern::{ParsedPatternItem, Parser, QuoteMode};
///
/// let input = "{0} 'and' {1}";
///
/// let mut parser = Parser::new(input, QuoteMode::QuotingSupported.into());
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
///             content: " ".into(),
///             quoted: false
///         },
///         ParsedPatternItem::Literal {
///             content: "and".into(),
///             quoted: true
///         },
///         ParsedPatternItem::Literal {
///             content: " ".into(),
///             quoted: false
///         },
///         ParsedPatternItem::Placeholder(1),
///     ]
/// );
/// ```
///
/// ## Fallible Iterator
///
/// Rust providers a strong support for iterators and iterator combinators, which
/// fits very well into the design of this parser/interpolator model.
///
/// Unfortunately, Rust iterators at the moment are infallible, while parsers are inhereantely
/// fallible. As such, the decision has been made to design the API in line with what
/// we hope will become a trait signature of a fallible iterator in the future, rather
/// than implementing a reversed infallible iterator (where the [`Item`] would be
/// `Option<Result<Item>>`).
///
/// That decision impacts the ergonomics of operating on the parser, on one hand making
/// the fallible iteration more ergonomic, at a trade-off of losing access to the wide
/// range of Rust iterator traits.
///
/// ## Generic Placeholder
///
/// To handle generic placeholder design, the only constrain necessary in the parser
/// is that a placeholder must be parsed from a string slice.
/// At the moment of writing, Rust is [preparing to deprecate][`RFC 2924`] [`FromStr`] in favor of
/// [`TryFrom<&str>`][`TryFrom`].
/// Among many benfits of such transition would be the auto-trait behavior of [`From`] and
/// a [`TryFrom<&str>`][`TryFrom`] for [`&str`] allowing for placeholders to be [`&str`] themselves.
///
/// Unfortunately, at the moment [`TryFrom<&str>`][`TryFrom`] for [`usize`] is not implemented, which would
/// impact the core use case of placeholder patterns.
///
/// In result, the decision has been made to use [`FromStr`] for the time being, until
/// [`TryFrom<&str>`][`TryFrom`] gets implemented on all types that support [`FromStr`].
///
/// [`TR35 2.6.1]: https://unicode.org/reports/tr35/tr35-dates.html#dateTimeFormat
/// [`RFC 2924`]: https://github.com/rust-lang/rfcs/pull/2924
/// [`Item`]: core::iter::Iterator::Item
/// [`TryFrom`]: core::convert::TryFrom
/// [`PlaceholderValueProvider`]: crate::PlaceholderValueProvider
#[derive(Debug)]
pub struct Parser<'p, P> {
    input: &'p str,
    len: usize,

    quote_mode: QuoteMode,

    start_idx: usize,
    idx: usize,

    state: ParserState,
    marker: PhantomData<P>,
}

impl<'p, P> Parser<'p, P> {
    /// Creates a new `Parser`.
    ///
    /// The `allow_raw_letters` controls whether the parser will support
    /// ASCII letters without quotes.
    ///
    /// # Examples
    /// ```
    /// use icu_pattern::{Parser, ParserOptions};
    /// let mut parser = Parser::<usize>::new("{0}, {1}", ParserOptions::default());
    /// ```
    pub fn new(input: &'p str, options: ParserOptions) -> Self {
        Self {
            input,
            len: input.len(),

            quote_mode: options.quote_mode,

            start_idx: 0,
            idx: 0,

            state: ParserState::default(),
            marker: PhantomData,
        }
    }

    /// An iterator method that advances the iterator and returns the result of an attempt to parse
    /// the next token.
    ///
    /// # Examples
    /// ```
    /// use icu_pattern::{ParsedPatternItem, Parser, ParserOptions};
    ///
    /// let mut parser = Parser::<usize>::new("{0}, {1}", ParserOptions::default());
    ///
    /// // A call to try_next() returns the next value…
    /// assert_eq!(
    ///     Ok(Some(ParsedPatternItem::Placeholder(0))),
    ///     parser.try_next()
    /// );
    /// assert_eq!(
    ///     Ok(Some(ParsedPatternItem::Literal {
    ///         content: ", ".into(),
    ///         quoted: false
    ///     })),
    ///     parser.try_next()
    /// );
    /// assert_eq!(
    ///     Ok(Some(ParsedPatternItem::Placeholder(1))),
    ///     parser.try_next()
    /// );
    ///
    /// // … and then `None` once it's over.
    /// assert_eq!(Ok(None), parser.try_next());
    /// ```
    pub fn try_next(
        &mut self,
    ) -> Result<Option<ParsedPatternItem<'p, P>>, ParserError<<P as FromStr>::Err>>
    where
        P: FromStr,
        P::Err: Debug,
    {
        while let Some(b) = self.input.as_bytes().get(self.idx) {
            match self.state {
                ParserState::Placeholder if *b == b'}' => {
                    let range = self.advance_state(self.idx, ParserState::Default);
                    return self.input[range]
                        .parse()
                        .map(|ret| Some(ParsedPatternItem::Placeholder(ret)))
                        .map_err(ParserError::InvalidPlaceholder);
                }
                ParserState::QuotedLiteral
                    if *b == b'\'' && self.quote_mode != QuoteMode::QuotesAreLiterals =>
                {
                    if self.input.as_bytes().get(self.idx + 1) == Some(&b'\'') {
                        handle_literal!(self, true, ParserState::Apostrophe { quoted: true })
                    } else {
                        handle_literal!(self, true, ParserState::Default)
                    }
                }
                ParserState::Default if *b == b'{' => {
                    handle_literal!(self, false, ParserState::Placeholder)
                }
                ParserState::Default
                    if *b == b'\'' && self.quote_mode != QuoteMode::QuotesAreLiterals =>
                {
                    if self.input.as_bytes().get(self.idx + 1) == Some(&b'\'') {
                        handle_literal!(self, false, ParserState::Apostrophe { quoted: false })
                    } else {
                        handle_literal!(self, false, ParserState::QuotedLiteral)
                    }
                }
                ParserState::Default
                    if self.quote_mode == QuoteMode::QuotingRequired && b.is_ascii_alphabetic() =>
                {
                    return Err(ParserError::IllegalCharacter(*b as char));
                }
                ParserState::Apostrophe { quoted } => {
                    self.start_idx -= 1;
                    if quoted {
                        handle_literal!(self, true, ParserState::QuotedLiteral)
                    } else {
                        handle_literal!(self, false, ParserState::Default)
                    }
                }
                _ => self.idx += 1,
            }
        }
        match self.state {
            ParserState::Placeholder => Err(ParserError::UnclosedPlaceholder),
            ParserState::QuotedLiteral => Err(ParserError::UnclosedQuotedLiteral),
            ParserState::Apostrophe { .. } => unreachable!(),
            ParserState::Default => {
                let range = self.start_idx..self.len;
                if !range.is_empty() {
                    self.start_idx = self.len;
                    Ok(Some(ParsedPatternItem::Literal {
                        content: Cow::Borrowed(&self.input[range]),
                        quoted: false,
                    }))
                } else {
                    Ok(None)
                }
            }
        }
    }

    fn advance_state(&mut self, idx: usize, next_state: ParserState) -> core::ops::Range<usize> {
        let range = self.start_idx..idx;
        self.idx = idx + 1;
        self.start_idx = self.idx;
        self.state = next_state;
        range
    }

    /// Mutates this parser and collects all [`ParsedPatternItem`]s into a vector.
    pub fn try_collect_into_vec(
        mut self,
    ) -> Result<Vec<ParsedPatternItem<'p, P>>, ParserError<<P as FromStr>::Err>>
    where
        P: FromStr,
        P::Err: Debug,
    {
        let mut result = vec![];
        while let Some(token) = self.try_next()? {
            result.push(token);
        }
        Ok(result)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::ops::Deref;

    #[test]
    fn pattern_parse_placeholders() {
        let samples = vec![
            ("{0}", vec![ParsedPatternItem::Placeholder(0)]),
            (
                "{0}{1}",
                vec![
                    ParsedPatternItem::Placeholder(0),
                    ParsedPatternItem::Placeholder(1),
                ],
            ),
            (
                "{0} 'at' {1}",
                vec![
                    ParsedPatternItem::Placeholder(0),
                    ParsedPatternItem::Literal {
                        content: " ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Literal {
                        content: "at".into(),
                        quoted: true,
                    },
                    ParsedPatternItem::Literal {
                        content: " ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Placeholder(1),
                ],
            ),
            (
                "{0}'at'{1}",
                vec![
                    ParsedPatternItem::Placeholder(0),
                    ParsedPatternItem::Literal {
                        content: "at".into(),
                        quoted: true,
                    },
                    ParsedPatternItem::Placeholder(1),
                ],
            ),
            (
                "'{0}' 'at' '{1}'",
                vec![
                    ParsedPatternItem::Literal {
                        content: "{0}".into(),
                        quoted: true,
                    },
                    ParsedPatternItem::Literal {
                        content: " ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Literal {
                        content: "at".into(),
                        quoted: true,
                    },
                    ParsedPatternItem::Literal {
                        content: " ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Literal {
                        content: "{1}".into(),
                        quoted: true,
                    },
                ],
            ),
            (
                "'PRE' {0} 'and' {1} 'POST'",
                vec![
                    ParsedPatternItem::Literal {
                        content: "PRE".into(),
                        quoted: true,
                    },
                    ParsedPatternItem::Literal {
                        content: " ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Placeholder(0),
                    ParsedPatternItem::Literal {
                        content: " ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Literal {
                        content: "and".into(),
                        quoted: true,
                    },
                    ParsedPatternItem::Literal {
                        content: " ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Placeholder(1),
                    ParsedPatternItem::Literal {
                        content: " ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Literal {
                        content: "POST".into(),
                        quoted: true,
                    },
                ],
            ),
            (
                "{0} o''clock and 'o''clock'",
                vec![
                    ParsedPatternItem::Placeholder(0),
                    ParsedPatternItem::Literal {
                        content: " o".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Literal {
                        content: "'".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Literal {
                        content: "clock and ".into(),
                        quoted: false,
                    },
                    ParsedPatternItem::Literal {
                        content: "o".into(),
                        quoted: true,
                    },
                    ParsedPatternItem::Literal {
                        content: "'".into(),
                        quoted: true,
                    },
                    ParsedPatternItem::Literal {
                        content: "clock".into(),
                        quoted: true,
                    },
                ],
            ),
        ];

        for (input, expected) in samples {
            let parser = Parser::new(input, QuoteMode::QuotingSupported.into());
            let result = parser
                .try_collect_into_vec()
                .expect("Failed to parse a pattern");
            assert_eq!(result.deref(), expected,);
        }

        let broken: Vec<(_, Option<ParserError<core::num::ParseIntError>>)> = vec![
            ("{", Some(ParserError::UnclosedPlaceholder)),
            ("{0", Some(ParserError::UnclosedPlaceholder)),
            ("{01", Some(ParserError::UnclosedPlaceholder)),
            (
                "{date}",
                // This should be:
                // ```
                // ParserError::InvalidPlaceholder(
                //     ParseIntError {
                //         kind: core::num::IntErrorKind::InvalidDigit
                //     }
                // ),
                // ```
                // Pending: https://github.com/rust-lang/rust/issues/22639
                //
                // Once that is fixed, we can stop using an `Option` here.
                None,
            ),
            ("{date} 'days'", None),
            ("'{00}", Some(ParserError::UnclosedQuotedLiteral)),
            ("d", Some(ParserError::IllegalCharacter('d'))),
        ];

        for (input, error) in broken {
            let parser = Parser::<usize>::new(input, QuoteMode::QuotingRequired.into());
            let result = parser.try_collect_into_vec();
            if let Some(error) = error {
                assert_eq!(result.expect_err("Should have failed."), error,);
            } else {
                assert!(result.is_err());
            }
        }
    }
}
