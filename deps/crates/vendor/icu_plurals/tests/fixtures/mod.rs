// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::Decimal;
#[cfg(feature = "unstable")]
use icu_plurals::PluralOperands;
use icu_plurals::{PluralCategory, PluralRuleType, PluralRulesOptions};
use serde::Deserialize;

/// Defines the data-driven test sets for the operands.
#[derive(Deserialize)]
#[allow(dead_code)]
pub struct OperandsTestSet {
    pub string: Vec<OperandsTest<String>>,
    pub int: Vec<OperandsTest<isize>>,
    pub floats: Vec<OperandsTest<f64>>,
    pub from_test: Vec<FromTestCase>,
}

/// A single test case verifying the conversion from [`FixedDecimal`] into
/// [`PluralOperands`].
#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct FromTestCase {
    /// The [`FixedDecimal`] input
    pub input: FixedDecimalInput,
    /// The expected value after conversion.
    pub expected: PluralOperandsInput,
}

/// A serialized representation of [`FixedDecimal`] in the data driven tests.
///
/// Use the `From` trait to convert into [`FixedDecimal`] in tests.
#[derive(Debug, Deserialize)]
pub struct FixedDecimalInput {
    /// Value supplied to [`FixedDecimal::from`] when constructing.
    from: i64,
    /// Value supplied to [`FixedDecimal::multiplied_pow10`] when constructing.
    pow10: i16,
}

impl From<&FixedDecimalInput> for Decimal {
    fn from(f: &FixedDecimalInput) -> Self {
        let mut dec = Decimal::from(f.from);
        dec.multiply_pow10(f.pow10);
        dec
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(untagged)]
pub enum PluralOperandsInput {
    List((u64, usize, usize, u64, u64, usize)),
    Struct {
        i: Option<u64>,
        v: Option<usize>,
        w: Option<usize>,
        f: Option<u64>,
        t: Option<u64>,
        c: Option<usize>,
    },
    String(String),
    Number(isize),
}

#[cfg(feature = "unstable")]
impl From<PluralOperandsInput> for PluralOperands {
    fn from(input: PluralOperandsInput) -> Self {
        use icu_plurals::RawPluralOperands;
        match input {
            PluralOperandsInput::List((i, v, w, f, t, c)) => {
                PluralOperands::from(RawPluralOperands { i, v, w, f, t, c })
            }
            PluralOperandsInput::Struct { i, v, w, f, t, c } => {
                PluralOperands::from(RawPluralOperands {
                    i: i.unwrap_or(0),
                    v: v.unwrap_or(0),
                    w: w.unwrap_or(0),
                    f: f.unwrap_or(0),
                    t: t.unwrap_or(0),
                    c: c.unwrap_or(0),
                })
            }
            PluralOperandsInput::String(num) => num
                .parse()
                .expect("Failed to parse a number into operands."),
            PluralOperandsInput::Number(num) => num.into(),
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[allow(dead_code)]
pub struct OperandsTest<T> {
    pub input: T,
    pub output: PluralOperandsInput,
}

#[derive(Deserialize)]
#[allow(dead_code)]
pub struct RuleTest {
    pub rule: String,
    pub input: PluralOperandsInput,
    pub output: RuleTestOutput,
}

#[derive(Deserialize)]
#[serde(untagged)]
#[allow(dead_code)]
pub enum RuleTestOutput {
    Value(bool),
    Error(String),
}

#[derive(Deserialize)]
#[allow(dead_code)]
pub struct RuleTestSet(pub Vec<RuleTest>);

#[derive(Clone, Copy, Debug, Deserialize)]
pub enum PluralRuleTypeInput {
    Cardinal,
    Ordinal,
}

impl From<PluralRuleTypeInput> for PluralRulesOptions {
    fn from(other: PluralRuleTypeInput) -> Self {
        match other {
            PluralRuleTypeInput::Cardinal => PluralRuleType::Cardinal.into(),
            PluralRuleTypeInput::Ordinal => PluralRuleType::Ordinal.into(),
        }
    }
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum PluralCategoryInput {
    Zero,
    One,
    Two,
    Few,
    Many,
    Other,
}

impl PartialEq<PluralCategory> for &PluralCategoryInput {
    fn eq(&self, other: &PluralCategory) -> bool {
        matches!(
            (self, other),
            (PluralCategoryInput::Zero, PluralCategory::Zero)
                | (PluralCategoryInput::One, PluralCategory::One)
                | (PluralCategoryInput::Two, PluralCategory::Two)
                | (PluralCategoryInput::Few, PluralCategory::Few)
                | (PluralCategoryInput::Many, PluralCategory::Many)
                | (PluralCategoryInput::Other, PluralCategory::Other)
        )
    }
}

#[derive(Deserialize)]
#[allow(dead_code)]
pub struct CategoriesTest {
    pub langid: String,
    pub plural_type: PluralRuleTypeInput,
    pub categories: Vec<PluralCategoryInput>,
}
