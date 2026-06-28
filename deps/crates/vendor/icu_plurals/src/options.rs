// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

/// A list of options set by the developer to adjust the behavior of the [`PluralRules`](crate::PluralRules).
///
/// # Examples
/// ```
/// use icu::plurals::{PluralRuleType, PluralRulesOptions};
///
/// let options =
///     PluralRulesOptions::default().with_type(PluralRuleType::Cardinal);
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub struct PluralRulesOptions {
    /// Plural rule type to use.
    ///
    /// Default is [`PluralRuleType::Cardinal`]
    pub rule_type: Option<PluralRuleType>,
}

impl Default for PluralRulesOptions {
    fn default() -> Self {
        Self::default()
    }
}

impl PluralRulesOptions {
    /// Constructs a new [`PluralRulesOptions`] struct.
    pub const fn default() -> Self {
        Self { rule_type: None }
    }

    /// Auguments the struct with the set [`PluralRuleType`].
    pub const fn with_type(mut self, rule_type: PluralRuleType) -> Self {
        self.rule_type = Some(rule_type);
        self
    }
}

impl From<PluralRuleType> for PluralRulesOptions {
    fn from(value: PluralRuleType) -> Self {
        Self {
            rule_type: Some(value),
        }
    }
}

/// A type of a plural rule which can be associated with the [`PluralRules`] struct.
///
/// [`PluralRules`]: crate::PluralRules
#[derive(Debug, PartialEq, Eq, Clone, Copy, Hash, Default)]
#[non_exhaustive]
pub enum PluralRuleType {
    /// Cardinal plural forms express quantities of units such as time, currency or distance,
    /// used in conjunction with a number expressed in decimal digits (i.e. "2", not "two").
    ///
    /// For example, English has two forms for cardinals:
    ///
    /// * [`One`]: `1 day`
    /// * [`Other`]: `0 days`, `2 days`, `10 days`, `0.3 days`
    ///
    /// [`One`]: crate::PluralCategory::One
    /// [`Other`]: crate::PluralCategory::Other
    #[default]
    Cardinal,
    /// Ordinal plural forms denote the order of items in a set and are always integers.
    ///
    /// For example, English has four forms for ordinals:
    ///
    /// * [`One`]: `1st floor`, `21st floor`, `101st floor`
    /// * [`Two`]: `2nd floor`, `22nd floor`, `102nd floor`
    /// * [`Few`]: `3rd floor`, `23rd floor`, `103rd floor`
    /// * [`Other`]: `4th floor`, `11th floor`, `96th floor`
    ///
    /// [`One`]: crate::PluralCategory::One
    /// [`Two`]: crate::PluralCategory::Two
    /// [`Few`]: crate::PluralCategory::Few
    /// [`Other`]: crate::PluralCategory::Other
    Ordinal,
}
