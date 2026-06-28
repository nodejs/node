// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::lazy_automaton::LazyAutomaton;
use crate::provider::*;
#[cfg(feature = "datagen")]
use alloc::string::ToString;
#[cfg(feature = "datagen")]
use icu_provider::DataError;
use writeable::{LengthHint, Writeable};
#[cfg(feature = "datagen")]
use zerovec::VarZeroCow;

impl ListFormatterPatterns<'_> {
    /// Creates a new [`ListFormatterPatterns`] from the given patterns. Fails if any pattern is invalid.
    #[cfg(feature = "datagen")]
    pub fn try_new(start: &str, middle: &str, end: &str, pair: &str) -> Result<Self, DataError> {
        use zerovec::VarZeroCow;

        let err = DataError::custom("Invalid list pattern");
        Ok(Self {
            start: ListJoinerPattern::try_from_str(start, true, false)?,
            middle: VarZeroCow::new_owned(
                middle
                    .strip_prefix("{0}")
                    .ok_or(err)?
                    .strip_suffix("{1}")
                    .ok_or(err)?
                    .to_string()
                    .into_boxed_str(),
            ),
            end: ListJoinerPattern::try_from_str(end, false, true)?.into(),
            pair: if end != pair {
                Some(ListJoinerPattern::try_from_str(pair, true, true)?.into())
            } else {
                None
            },
        })
    }

    /// The range of the number of bytes required by the list literals to join a
    /// list of length `len`. If none of the patterns are conditional, this is exact.
    pub(crate) fn length_hint(&self, len: usize) -> LengthHint {
        match len {
            0 | 1 => LengthHint::exact(0),
            2 => self.pair.as_ref().unwrap_or(&self.end).size_hint(),
            n => {
                self.start.size_hint()
                    + self.middle.writeable_length_hint() * (n - 3)
                    + self.end.size_hint()
            }
        }
    }
}

type PatternParts<'a> = (&'a str, &'a str, &'a str);

impl<'a> ConditionalListJoinerPattern<'a> {
    pub(crate) fn parts<'b, W: Writeable + ?Sized>(
        &'a self,
        following_value: &'b W,
    ) -> PatternParts<'a> {
        match &self.special_case {
            Some(SpecialCasePattern { condition, pattern })
                if condition.deref().matches_earliest_fwd_lazy(following_value) =>
            {
                pattern.parts()
            }
            _ => self.default.parts(),
        }
    }

    /// The expected length of this pattern
    fn size_hint(&'a self) -> LengthHint {
        let mut hint = self.default.size_hint();
        if let Some(special_case) = &self.special_case {
            hint |= special_case.pattern.size_hint()
        }
        hint
    }
}

impl<'data> ListJoinerPattern<'data> {
    #[cfg(feature = "datagen")]
    /// Parses a [`ListJoinerPattern`] from a string containing the "{0}" and "{1}" placeholders.
    pub fn try_from_str(
        pattern: &str,
        allow_prefix: bool,
        allow_suffix: bool,
    ) -> Result<Self, DataError> {
        match (pattern.find("{0}"), pattern.find("{1}")) {
            (Some(index_0), Some(index_1))
                if index_0 < index_1
                    && (allow_prefix || index_0 == 0)
                    && (allow_suffix || index_1 == pattern.len() - 3) =>
            {
                if (index_0 > 0 && !cfg!(test)) || index_1 - 3 >= 256 {
                    return Err(DataError::custom(
                        "Found valid pattern that cannot be stored in ListFormatterPatterns",
                    )
                    .with_debug_context(pattern));
                }
                Ok(ListJoinerPattern {
                    string: VarZeroCow::new_owned(
                        alloc::format!(
                            "{}{}{}",
                            &pattern[0..index_0],
                            &pattern[index_0 + 3..index_1],
                            &pattern[index_1 + 3..]
                        )
                        .into_boxed_str(),
                    ),
                    index_0: index_0 as u8,
                    index_1: (index_1 - 3) as u8,
                })
            }
            _ => Err(DataError::custom("Invalid list pattern").with_debug_context(pattern)),
        }
    }

    pub(crate) fn parts(&'data self) -> PatternParts<'data> {
        #![allow(clippy::indexing_slicing)] // by invariant
        let index_0 = self.index_0 as usize;
        let index_1 = self.index_1 as usize;
        (
            &self.string[0..index_0],
            &self.string[index_0..index_1],
            &self.string[index_1..],
        )
    }

    fn size_hint(&self) -> LengthHint {
        LengthHint::exact(self.string.len())
    }
}

#[cfg(feature = "datagen")]
impl<'data> From<ListJoinerPattern<'data>> for ConditionalListJoinerPattern<'data> {
    fn from(default: ListJoinerPattern<'data>) -> Self {
        Self {
            default,
            special_case: None,
        }
    }
}

#[cfg(all(test, feature = "datagen"))]
pub mod test {
    use super::*;
    use std::borrow::Cow;

    pub fn test_patterns_general() -> ListFormatterPatterns<'static> {
        ListFormatterPatterns::try_new("@{0}:{1}", "{0},{1}", "{0}.{1}!", "${0};{1}+").unwrap()
    }

    pub fn test_patterns_lengths() -> ListFormatterPatterns<'static> {
        ListFormatterPatterns::try_new("{0}1{1}", "{0}12{1}", "{0}12{1}34", "{0}123{1}456").unwrap()
    }

    pub fn test_patterns_conditional() -> ListFormatterPatterns<'static> {
        let mut patterns =
            ListFormatterPatterns::try_new("{0}: {1}", "{0}, {1}", "{0}. {1}", "{0}. {1}").unwrap();
        patterns.end.special_case = Some(SpecialCasePattern {
            condition: SerdeDFA::new(Cow::Borrowed("^a")).unwrap(),
            pattern: ListJoinerPattern::try_from_str("{0} :o {1}", false, false).unwrap(),
        });
        patterns
    }

    #[test]
    fn rejects_bad_patterns() {
        assert!(ListJoinerPattern::try_from_str("{0} and", true, true).is_err());
        assert!(ListJoinerPattern::try_from_str("and {1}", true, true).is_err());
        assert!(ListJoinerPattern::try_from_str("{1} and {0}", true, true).is_err());
        assert!(ListJoinerPattern::try_from_str("{1{0}}", true, true).is_err());
        assert!(ListJoinerPattern::try_from_str("{0\u{202e}} and {1}", true, true).is_err());
        assert!(ListJoinerPattern::try_from_str("{{0}} {{1}}", true, true).is_ok());

        assert!(ListJoinerPattern::try_from_str("{0} and {1} ", true, true).is_ok());
        assert!(ListJoinerPattern::try_from_str("{0} and {1} ", true, false).is_err());
        assert!(ListJoinerPattern::try_from_str(" {0} and {1}", true, true).is_ok());
        assert!(ListJoinerPattern::try_from_str(" {0} and {1}", false, true).is_err());
    }

    #[test]
    fn produces_correct_parts() {
        assert_eq!(
            test_patterns_general().pair.unwrap().parts(""),
            ("$", ";", "+")
        );
    }

    #[test]
    fn produces_correct_parts_conditionally() {
        assert_eq!(test_patterns_conditional().end.parts("a"), ("", " :o ", ""));
        assert_eq!(
            test_patterns_conditional().end.parts("ab"),
            ("", " :o ", "")
        );
        assert_eq!(test_patterns_conditional().end.parts("b"), ("", ". ", ""));
        assert_eq!(test_patterns_conditional().end.parts("ba"), ("", ". ", ""));
    }

    #[test]
    fn size_hint_works() {
        let pattern = test_patterns_lengths();

        assert_eq!(pattern.length_hint(0), LengthHint::exact(0));
        assert_eq!(pattern.length_hint(1), LengthHint::exact(0));

        // pair pattern "{0}123{1}456"
        assert_eq!(pattern.length_hint(2), LengthHint::exact(6));

        // patterns "{0}1{1}", "{0}12{1}" (x197), and "{0}12{1}34"
        assert_eq!(pattern.length_hint(200), LengthHint::exact(1 + 2 * 197 + 4));

        let pattern = test_patterns_conditional();

        // patterns "{0}: {1}", "{0}, {1}" (x197), and "{0} :o {1}" or "{0}. {1}"
        assert_eq!(
            pattern.length_hint(200),
            LengthHint::exact(2 + 197 * 2) + LengthHint::between(2, 4)
        );
    }
}
