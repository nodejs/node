// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_locale_core::LanguageIdentifier;
use icu_plurals::PluralCategory;

use serde::Deserialize;

#[derive(Deserialize)]
#[allow(dead_code)]
pub struct NumbersFixture {
    pub isize: Vec<i64>,
    pub usize: Vec<u64>,
    pub string: Vec<String>,
    pub string_samples: Vec<String>,
    pub fixed_decimals: Vec<FromFixedDecimals>,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct PluralsFixture {
    pub langs: Vec<LanguageIdentifier>,
}

/// Describes a number to construct from plural operands, as `value * 10^(exponent)`.  Construction
/// from value and exponent is because sometimes we want to preserve trailing zeros.
#[derive(Deserialize)]
#[allow(dead_code)]
pub struct FromFixedDecimals {
    pub value: i64,
    pub exponent: i16,
}

#[derive(Debug, Deserialize)]
pub struct LocalePluralRulesFixture {
    #[serde(rename = "pluralRule-count-zero")]
    pub zero: Option<String>,
    #[serde(rename = "pluralRule-count-one")]
    pub one: Option<String>,
    #[serde(rename = "pluralRule-count-two")]
    pub two: Option<String>,
    #[serde(rename = "pluralRule-count-few")]
    pub few: Option<String>,
    #[serde(rename = "pluralRule-count-many")]
    pub many: Option<String>,
}

impl LocalePluralRulesFixture {
    #[allow(dead_code)]
    pub fn get(&self, category: PluralCategory) -> Option<&String> {
        match category {
            PluralCategory::Zero => self.zero.as_ref(),
            PluralCategory::One => self.one.as_ref(),
            PluralCategory::Two => self.two.as_ref(),
            PluralCategory::Few => self.few.as_ref(),
            PluralCategory::Many => self.many.as_ref(),
            PluralCategory::Other => None,
        }
    }
}
