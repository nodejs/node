// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::{fmt, str::FromStr};

use crate::{ParsedPatternItem, Parser, PatternError, PatternItemCow};

impl<'a, K> From<ParsedPatternItem<'a, K>> for PatternItemCow<'a, K> {
    fn from(value: ParsedPatternItem<'a, K>) -> Self {
        match value {
            ParsedPatternItem::Literal { content, .. } => PatternItemCow::Literal(content),
            ParsedPatternItem::Placeholder(key) => PatternItemCow::Placeholder(key),
        }
    }
}

impl<'a, K> Iterator for Parser<'a, K>
where
    K: FromStr,
    K::Err: fmt::Debug,
{
    type Item = Result<PatternItemCow<'a, K>, PatternError>;

    fn next(&mut self) -> Option<Self::Item> {
        match self.try_next() {
            Ok(Some(pattern_token)) => Some(Ok(pattern_token.into())),
            Ok(None) => None,
            Err(_e) => Some(Err(PatternError::InvalidPattern)),
        }
    }
}
