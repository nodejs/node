// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, doc)), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
    )
)]
#![warn(missing_docs)]

//! Determine the plural category appropriate for a given number in a given language.
//!
//! This module is published as its own crate ([`icu_plurals`](https://docs.rs/icu_plurals/latest/icu_plurals/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! For example in English, when constructing a message
//! such as `{ num } items`, the user has to prepare
//! two variants of the message:
//!
//! * `1 item`
//! * `0 items`, `2 items`, `5 items`, `0.5 items` etc.
//!
//! The former variant is used when the placeholder variable has value `1`,
//! while the latter is used for all other values of the variable.
//!
//! Unicode defines [Language Plural Rules] as a mechanism to codify those
//! variants and provides data and algorithms to calculate
//! appropriate [`PluralCategory`].
//!
//! # Examples
//!
//! ```
//! use icu::locale::locale;
//! use icu::plurals::{PluralCategory, PluralRules};
//!
//! let pr = PluralRules::try_new(locale!("en").into(), Default::default())
//!     .expect("locale should be present");
//!
//! assert_eq!(pr.category_for(5_usize), PluralCategory::Other);
//! ```
//!
//! ## Plural Rules
//!
//! The crate provides the main struct [`PluralRules`] which handles selection
//! of the correct [`PluralCategory`] for a given language and [`PluralRuleType`].
//!
//! ## Plural Category
//!
//! Every number in every language belongs to a certain [`PluralCategory`].
//! For example, the Polish language uses four:
//!
//! * [`One`](PluralCategory::One): `1 miesiąc`
//! * [`Few`](PluralCategory::Few): `2 miesiące`
//! * [`Many`](PluralCategory::Many): `5 miesięcy`
//! * [`Other`](PluralCategory::Other): `1.5 miesiąca`
//!
//! ## `PluralRuleType`
//!
//! Plural rules depend on the use case. This crate supports two types of plural rules:
//!
//! * [`Cardinal`](PluralRuleType::Cardinal): `3 doors`, `1 month`, `10 dollars`
//! * [`Ordinal`](PluralRuleType::Ordinal): `1st place`, `10th day`, `11th floor`
//!
//! [Language Plural Rules]: https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules

extern crate alloc;

mod operands;
mod options;
pub mod provider;

// Need to expose it for datagen, but we don't
// have a reason to make it fully public, so hiding docs for now.
#[cfg(feature = "unstable")]
mod raw_operands;

#[cfg(feature = "unstable")]
pub use raw_operands::RawPluralOperands;

use core::cmp::{Ord, PartialOrd};
use core::convert::Infallible;
use icu_locale_core::preferences::define_preferences;
use icu_provider::marker::ErasedMarker;
use icu_provider::prelude::*;
pub use operands::PluralOperands;
pub use options::*;
use provider::rules::runtime::test_rule;
use provider::PluralRulesData;
use provider::PluralsCardinalV1;
use provider::PluralsOrdinalV1;

#[cfg(feature = "unstable")]
use provider::PluralsRangesV1;
#[cfg(feature = "unstable")]
use provider::UnvalidatedPluralRange;

/// The plural categories are used to format messages with numeric placeholders, expressed as decimal numbers.
///
/// The fundamental rule for determining plural categories is the existence of minimal pairs: whenever two different
/// numbers may require different versions of the same message, then the numbers have different plural categories.
///
/// All languages supported by `ICU4X` can match any number to one of the categories.
///
/// # Examples
///
/// ```
/// use icu::locale::locale;
/// use icu::plurals::{PluralCategory, PluralRules};
///
/// let pr = PluralRules::try_new(locale!("en").into(), Default::default())
///     .expect("locale should be present");
///
/// assert_eq!(pr.category_for(5_usize), PluralCategory::Other);
/// ```
#[derive(Debug, PartialEq, Eq, Clone, Copy, Hash, Ord, PartialOrd)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_plurals))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[repr(u8)]
#[zerovec::make_ule(PluralCategoryULE)]
#[allow(clippy::exhaustive_enums)] // this type is mostly stable. new categories may potentially be added in the future,
                                   // but at a cadence slower than the ICU4X release cycle
pub enum PluralCategory {
    /// CLDR "zero" plural category. Used in Arabic and Latvian, among others.
    ///
    /// Examples of numbers having this category:
    ///
    /// - 0 in Arabic (ar), Latvian (lv)
    /// - 10~20, 30, 40, 50, ... in Latvian (lv)
    Zero = 0,
    /// CLDR "one" plural category. Signifies the singular form in many languages.
    ///
    /// Examples of numbers having this category:
    ///
    /// - 0 in French (fr), Portuguese (pt), ...
    /// - 1 in English (en) and most other languages
    /// - 2.1 in Filipino (fil), Croatian (hr), Latvian (lv), Serbian (sr)
    /// - 2, 3, 5, 7, 8, ... in Filipino (fil)
    One = 1,
    /// CLDR "two" plural category. Used in Arabic, Hebrew, and Slovenian, among others.
    ///
    /// Examples of numbers having this category:
    ///
    /// - 2 in Arabic (ar), Hebrew (iw), Slovenian (sl)
    /// - 2.0 in Arabic (ar)
    Two = 2,
    /// CLDR "few" plural category. Used in Romanian, Polish, Russian, and others.
    ///
    /// Examples of numbers having this category:
    ///
    /// - 0 in Romanian (ro)
    /// - 1.2 in Croatian (hr), Romanian (ro), Slovenian (sl), Serbian (sr)
    /// - 2 in Polish (pl), Russian (ru), Czech (cs), ...
    /// - 5 in Arabic (ar), Lithuanian (lt), Romanian (ro)
    Few = 3,
    /// CLDR "many" plural category. Used in Polish, Russian, Ukrainian, and others.
    ///
    /// Examples of numbers having this category:
    ///
    /// - 0 in Polish (pl)
    /// - 1.0 in Czech (cs), Slovak (sk)
    /// - 1.1 in Czech (cs), Lithuanian (lt), Slovak (sk)
    /// - 15 in Arabic (ar), Polish (pl), Russian (ru), Ukrainian (uk)
    Many = 4,
    /// CLDR "other" plural category, used as a catch-all. Each language supports it, and it
    /// is also used as a fail safe result for in case no better match can be identified.
    ///
    /// In some languages, such as Japanese, Chinese, Korean, and Thai, this is the only
    /// plural category.
    ///
    /// Examples of numbers having this category:
    ///
    /// - 0 in English (en), German (de), Spanish (es), ...
    /// - 1 in Japanese (ja), Korean (ko), Chinese (zh), Thai (th), ...
    /// - 2 in English (en), German (de), Spanish (es), ...
    Other = 5,
}

impl PluralCategory {
    /// Returns an ordered iterator over variants of [`Plural Categories`].
    ///
    /// Categories are returned in alphabetical order.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::plurals::PluralCategory;
    ///
    /// let mut categories = PluralCategory::all();
    ///
    /// assert_eq!(categories.next(), Some(PluralCategory::Few));
    /// assert_eq!(categories.next(), Some(PluralCategory::Many));
    /// assert_eq!(categories.next(), Some(PluralCategory::One));
    /// assert_eq!(categories.next(), Some(PluralCategory::Other));
    /// assert_eq!(categories.next(), Some(PluralCategory::Two));
    /// assert_eq!(categories.next(), Some(PluralCategory::Zero));
    /// assert_eq!(categories.next(), None);
    /// ```
    ///
    /// [`Plural Categories`]: PluralCategory
    pub fn all() -> impl ExactSizeIterator<Item = Self> {
        [
            Self::Few,
            Self::Many,
            Self::One,
            Self::Other,
            Self::Two,
            Self::Zero,
        ]
        .iter()
        .copied()
    }

    /// Returns the [`PluralCategory`] corresponding to given TR35 string.
    pub fn get_for_cldr_string(category: &str) -> Option<PluralCategory> {
        Self::get_for_cldr_bytes(category.as_bytes())
    }
    /// Returns the [`PluralCategory`] corresponding to given TR35 string as bytes
    pub fn get_for_cldr_bytes(category: &[u8]) -> Option<PluralCategory> {
        match category {
            b"zero" => Some(PluralCategory::Zero),
            b"one" => Some(PluralCategory::One),
            b"two" => Some(PluralCategory::Two),
            b"few" => Some(PluralCategory::Few),
            b"many" => Some(PluralCategory::Many),
            b"other" => Some(PluralCategory::Other),
            _ => None,
        }
    }
}

define_preferences!(
    /// The preferences for plural rules.
    [Copy]
    PluralRulesPreferences,
    {}
);

/// A struct which provides an ability to retrieve an appropriate
/// [`Plural Category`] for a given number.
///
/// # Examples
///
/// ```
/// use icu::locale::locale;
/// use icu::plurals::{PluralCategory, PluralRules};
///
/// let pr = PluralRules::try_new(locale!("en").into(), Default::default())
///     .expect("locale should be present");
///
/// assert_eq!(pr.category_for(5_usize), PluralCategory::Other);
/// ```
///
/// [`ICU4X`]: ../icu/index.html
/// [`Plural Type`]: PluralRuleType
/// [`Plural Category`]: PluralCategory
#[derive(Debug)]
pub struct PluralRules(DataPayload<ErasedMarker<PluralRulesData<'static>>>);

impl AsRef<PluralRules> for PluralRules {
    fn as_ref(&self) -> &PluralRules {
        self
    }
}

impl PluralRules {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: PluralRulesPreferences, options: PluralRulesOptions) -> error: DataError,
        /// Constructs a new `PluralRules` for a given locale and type using compiled data.
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::locale;
        /// use icu::plurals::PluralRules;
        ///
        /// let _ = PluralRules::try_new(
        ///     locale!("en").into(),
        ///     Default::default(),
        /// ).expect("locale should be present");
        /// ```
        ///
        /// [`data provider`]: icu_provider
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable(
        provider: &(impl DataProvider<PluralsCardinalV1> + DataProvider<PluralsOrdinalV1> + ?Sized),
        prefs: PluralRulesPreferences,
        options: PluralRulesOptions,
    ) -> Result<Self, DataError> {
        match options.rule_type.unwrap_or_default() {
            PluralRuleType::Cardinal => Self::try_new_cardinal_unstable(provider, prefs),
            PluralRuleType::Ordinal => Self::try_new_ordinal_unstable(provider, prefs),
        }
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: PluralRulesPreferences) -> error: DataError,
        /// Constructs a new `PluralRules` for a given locale for cardinal numbers using compiled data.
        ///
        /// Cardinal plural forms express quantities of units such as time, currency or distance,
        /// used in conjunction with a number expressed in decimal digits (i.e. "2", not "two").
        ///
        /// For example, English has two forms for cardinals:
        ///
        /// * [`One`]: `1 day`
        /// * [`Other`]: `0 days`, `2 days`, `10 days`, `0.3 days`
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::locale;
        /// use icu::plurals::{PluralCategory, PluralRules};
        ///
        /// let rules = PluralRules::try_new_cardinal(locale!("ru").into()).expect("locale should be present");
        ///
        /// assert_eq!(rules.category_for(2_usize), PluralCategory::Few);
        /// ```
        ///
        /// [`One`]: PluralCategory::One
        /// [`Other`]: PluralCategory::Other
        functions: [
            try_new_cardinal,
            try_new_cardinal_with_buffer_provider,
            try_new_cardinal_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_cardinal)]
    pub fn try_new_cardinal_unstable(
        provider: &(impl DataProvider<PluralsCardinalV1> + ?Sized),
        prefs: PluralRulesPreferences,
    ) -> Result<Self, DataError> {
        let locale = PluralsCardinalV1::make_locale(prefs.locale_preferences);
        Ok(Self(
            provider
                .load(DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                })?
                .payload
                .cast(),
        ))
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: PluralRulesPreferences) -> error: DataError,
        /// Constructs a new `PluralRules` for a given locale for ordinal numbers using compiled data.
        ///
        /// Ordinal plural forms denote the order of items in a set and are always integers.
        ///
        /// For example, English has four forms for ordinals:
        ///
        /// * [`One`]: `1st floor`, `21st floor`, `101st floor`
        /// * [`Two`]: `2nd floor`, `22nd floor`, `102nd floor`
        /// * [`Few`]: `3rd floor`, `23rd floor`, `103rd floor`
        /// * [`Other`]: `4th floor`, `11th floor`, `96th floor`
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::locale;
        /// use icu::plurals::{PluralCategory, PluralRules};
        ///
        /// let rules = PluralRules::try_new_ordinal(
        ///     locale!("ru").into(),
        /// )
        /// .expect("locale should be present");
        ///
        /// assert_eq!(rules.category_for(2_usize), PluralCategory::Other);
        /// ```
        ///
        /// [`One`]: PluralCategory::One
        /// [`Two`]: PluralCategory::Two
        /// [`Few`]: PluralCategory::Few
        /// [`Other`]: PluralCategory::Other
        functions: [
            try_new_ordinal,
            try_new_ordinal_with_buffer_provider,
            try_new_ordinal_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_ordinal)]
    pub fn try_new_ordinal_unstable(
        provider: &(impl DataProvider<PluralsOrdinalV1> + ?Sized),
        prefs: PluralRulesPreferences,
    ) -> Result<Self, DataError> {
        let locale = PluralsOrdinalV1::make_locale(prefs.locale_preferences);
        Ok(Self(
            provider
                .load(DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                })?
                .payload
                .cast(),
        ))
    }

    /// Returns the [`Plural Category`] appropriate for the given number.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::locale;
    /// use icu::plurals::{PluralCategory, PluralRules};
    ///
    /// let pr = PluralRules::try_new(locale!("en").into(), Default::default())
    ///     .expect("locale should be present");
    ///
    /// match pr.category_for(1_usize) {
    ///     PluralCategory::One => "One item",
    ///     PluralCategory::Other => "Many items",
    ///     _ => unreachable!(),
    /// };
    /// ```
    ///
    /// The method accepts any input that can be calculated into [`Plural Operands`].
    /// All unsigned primitive number types can infallibly be converted so they can be
    /// used as an input.
    ///
    /// For signed numbers and strings, [`Plural Operands`] implement [`TryFrom`]
    /// and [`FromStr`](std::str::FromStr), which should be used before passing the result to
    /// [`category_for()`](PluralRules::category_for()).
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::locale;
    /// use icu::plurals::{PluralRules, PluralCategory, PluralOperands};
    /// #
    /// # let pr = PluralRules::try_new(locale!("en").into(), Default::default())
    /// #     .expect("locale should be present");
    ///
    /// let operands = PluralOperands::from(-5);
    /// let operands2: PluralOperands = "5.10".parse().expect("Failed to parse to operands.");
    ///
    /// assert_eq!(pr.category_for(operands), PluralCategory::Other);
    /// assert_eq!(pr.category_for(operands2), PluralCategory::Other);
    /// ```
    ///
    /// [`Plural Category`]: PluralCategory
    /// [`Plural Operands`]: operands::PluralOperands
    pub fn category_for<I: Into<PluralOperands>>(&self, input: I) -> PluralCategory {
        let rules = self.0.get();
        let input = input.into();

        macro_rules! test_rule {
            ($rule:ident, $cat:ident) => {
                rules
                    .$rule
                    .as_ref()
                    .and_then(|r| test_rule(r, &input).then(|| PluralCategory::$cat))
            };
        }

        test_rule!(zero, Zero)
            .or_else(|| test_rule!(one, One))
            .or_else(|| test_rule!(two, Two))
            .or_else(|| test_rule!(few, Few))
            .or_else(|| test_rule!(many, Many))
            .unwrap_or(PluralCategory::Other)
    }

    /// Returns all [`Plural Categories`] appropriate for a [`PluralRules`] object
    /// based on the [`LanguageIdentifier`](icu::locale::{LanguageIdentifier}) and [`PluralRuleType`].
    ///
    /// The [`Plural Categories`] are returned in UTS 35 sorted order.
    ///
    /// The category [`PluralCategory::Other`] is always included.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::locale;
    /// use icu::plurals::{PluralCategory, PluralRules};
    ///
    /// let pr = PluralRules::try_new(locale!("fr").into(), Default::default())
    ///     .expect("locale should be present");
    ///
    /// let mut categories = pr.categories();
    /// assert_eq!(categories.next(), Some(PluralCategory::One));
    /// assert_eq!(categories.next(), Some(PluralCategory::Many));
    /// assert_eq!(categories.next(), Some(PluralCategory::Other));
    /// assert_eq!(categories.next(), None);
    /// ```
    ///
    /// [`Plural Categories`]: PluralCategory
    pub fn categories(&self) -> impl Iterator<Item = PluralCategory> + '_ {
        let rules = self.0.get();

        macro_rules! test_rule {
            ($rule:ident, $cat:ident) => {
                rules
                    .$rule
                    .as_ref()
                    .map(|_| PluralCategory::$cat)
                    .into_iter()
            };
        }

        test_rule!(zero, Zero)
            .chain(test_rule!(one, One))
            .chain(test_rule!(two, Two))
            .chain(test_rule!(few, Few))
            .chain(test_rule!(many, Many))
            .chain(Some(PluralCategory::Other))
    }
}

/// A [`PluralRules`] that also has the ability to retrieve an appropriate [`Plural Category`] for a
/// range.
///
/// ✨ *Enabled with the `unstable` Cargo feature.*
///
/// <div class="stab unstable">
/// 🚧 This code is unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/4140">#4140</a>
/// </div>
///
/// # Examples
///
/// ```
/// use icu::locale::locale;
/// use icu::plurals::{PluralCategory, PluralOperands, PluralRulesWithRanges};
///
/// let ranges = PluralRulesWithRanges::try_new(
///     locale!("ar").into(),
///     Default::default(),
/// )
/// .expect("locale should be present");
///
/// let operands = PluralOperands::from(1_usize);
/// let operands2: PluralOperands =
///     "2.0".parse().expect("parsing to operands should succeed");
///
/// assert_eq!(
///     ranges.category_for_range(operands, operands2),
///     PluralCategory::Other
/// );
/// ```
///
/// [`Plural Category`]: PluralCategory
#[cfg(feature = "unstable")]
#[derive(Debug)]
pub struct PluralRulesWithRanges<R> {
    rules: R,
    ranges: DataPayload<PluralsRangesV1>,
}

#[cfg(feature = "unstable")]
impl PluralRulesWithRanges<PluralRules> {
    icu_provider::gen_buffer_data_constructors!(

        (prefs: PluralRulesPreferences, options: PluralRulesOptions) -> error: DataError,
        /// Constructs a new `PluralRulesWithRanges` for a given locale using compiled data.
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::locale;
        /// use icu::plurals::PluralRulesWithRanges;
        ///
        /// let _ = PluralRulesWithRanges::try_new(
        ///     locale!("en").into(),
        ///     Default::default(),
        /// ).expect("locale should be present");
        /// ```
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable(
        provider: &(impl DataProvider<PluralsRangesV1>
              + DataProvider<PluralsCardinalV1>
              + DataProvider<PluralsOrdinalV1>
              + ?Sized),
        prefs: PluralRulesPreferences,
        options: PluralRulesOptions,
    ) -> Result<Self, DataError> {
        match options.rule_type.unwrap_or_default() {
            PluralRuleType::Cardinal => Self::try_new_cardinal_unstable(provider, prefs),
            PluralRuleType::Ordinal => Self::try_new_ordinal_unstable(provider, prefs),
        }
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: PluralRulesPreferences) -> error: DataError,
        /// Constructs a new `PluralRulesWithRanges` for a given locale for cardinal numbers using
        /// compiled data.
        ///
        /// See [`PluralRules::try_new_cardinal`] for more information.
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::locale;
        /// use icu::plurals::{PluralCategory, PluralRulesWithRanges};
        ///
        /// let rules = PluralRulesWithRanges::try_new_cardinal(locale!("ru").into())
        ///     .expect("locale should be present");
        ///
        /// assert_eq!(rules.category_for_range(0_usize, 2_usize), PluralCategory::Few);
        /// ```
        functions: [
            try_new_cardinal,
            try_new_cardinal_with_buffer_provider,
            try_new_cardinal_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_cardinal)]
    pub fn try_new_cardinal_unstable(
        provider: &(impl DataProvider<PluralsCardinalV1> + DataProvider<PluralsRangesV1> + ?Sized),
        prefs: PluralRulesPreferences,
    ) -> Result<Self, DataError> {
        let rules = PluralRules::try_new_cardinal_unstable(provider, prefs)?;

        PluralRulesWithRanges::try_new_with_rules_unstable(provider, prefs, rules)
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: PluralRulesPreferences) -> error: DataError,
        /// Constructs a new `PluralRulesWithRanges` for a given locale for ordinal numbers using
        /// compiled data.
        ///
        /// See [`PluralRules::try_new_ordinal`] for more information.
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::locale;
        /// use icu::plurals::{PluralCategory, PluralRulesWithRanges};
        ///
        /// let rules = PluralRulesWithRanges::try_new_ordinal(
        ///     locale!("ru").into(),
        /// )
        /// .expect("locale should be present");
        ///
        /// assert_eq!(rules.category_for_range(0_usize, 2_usize), PluralCategory::Other);
        /// ```
        functions: [
            try_new_ordinal,
            try_new_ordinal_with_buffer_provider,
            try_new_ordinal_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_ordinal)]
    pub fn try_new_ordinal_unstable(
        provider: &(impl DataProvider<PluralsOrdinalV1> + DataProvider<PluralsRangesV1> + ?Sized),
        prefs: PluralRulesPreferences,
    ) -> Result<Self, DataError> {
        let rules = PluralRules::try_new_ordinal_unstable(provider, prefs)?;

        PluralRulesWithRanges::try_new_with_rules_unstable(provider, prefs, rules)
    }
}

#[cfg(feature = "unstable")]
impl<R> PluralRulesWithRanges<R>
where
    R: AsRef<PluralRules>,
{
    icu_provider::gen_buffer_data_constructors!(
        (prefs: PluralRulesPreferences, rules: R) -> error: DataError,
        /// Constructs a new `PluralRulesWithRanges` for a given locale from an existing
        /// `PluralRules` (either owned or as a reference) and compiled data.
        ///
        /// # ⚠️ Warning
        ///
        /// The provided `locale` **MUST** be the same as the locale provided to the constructor
        /// of `rules`. Otherwise, [`Self::category_for_range`] will return incorrect results.
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::locale;
        /// use icu::plurals::{PluralRulesWithRanges, PluralRules};
        ///
        /// let rules = PluralRules::try_new(locale!("en").into(), Default::default())
        ///     .expect("locale should be present");
        ///
        /// let _ =
        ///     PluralRulesWithRanges::try_new_with_rules(locale!("en").into(), rules)
        ///         .expect("locale should be present");
        /// ```
        functions: [
            try_new_with_rules,
            try_new_with_rules_with_buffer_provider,
            try_new_with_rules_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_with_rules)]
    pub fn try_new_with_rules_unstable(
        provider: &(impl DataProvider<PluralsRangesV1> + ?Sized),
        prefs: PluralRulesPreferences,
        rules: R,
    ) -> Result<Self, DataError> {
        let locale = PluralsRangesV1::make_locale(prefs.locale_preferences);
        let ranges = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self { rules, ranges })
    }

    /// Gets a reference to the inner `PluralRules`.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::locale;
    /// use icu::plurals::{PluralCategory, PluralRulesWithRanges};
    ///
    /// let ranges = PluralRulesWithRanges::try_new_cardinal(locale!("en").into())
    ///     .expect("locale should be present");
    ///
    /// let rules = ranges.rules();
    ///
    /// assert_eq!(rules.category_for(1u8), PluralCategory::One);
    /// ```
    pub fn rules(&self) -> &PluralRules {
        self.rules.as_ref()
    }

    /// Returns the [`Plural Category`] appropriate for a range.
    ///
    /// Note that the returned category is correct only if the range fulfills the following requirements:
    /// - The start value is strictly less than the end value.
    /// - Both values are positive.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::locale;
    /// use icu::plurals::{PluralCategory, PluralOperands, PluralRulesWithRanges};
    ///
    /// let ranges = PluralRulesWithRanges::try_new(
    ///     locale!("ro").into(),
    ///     Default::default(),
    /// )
    /// .expect("locale should be present");
    /// let operands: PluralOperands =
    ///     "0.5".parse().expect("parsing to operands should succeed");
    /// let operands2 = PluralOperands::from(1_usize);
    ///
    /// assert_eq!(
    ///     ranges.category_for_range(operands, operands2),
    ///     PluralCategory::Few
    /// );
    /// ```
    ///
    /// [`Plural Category`]: PluralCategory
    pub fn category_for_range<S: Into<PluralOperands>, E: Into<PluralOperands>>(
        &self,
        start: S,
        end: E,
    ) -> PluralCategory {
        let rules = self.rules.as_ref();
        let start = rules.category_for(start);
        let end = rules.category_for(end);

        self.resolve_range(start, end)
    }

    /// Returns the [`Plural Category`] appropriate for a range from the categories of its endpoints.
    ///
    /// Note that the returned category is correct only if the original numeric range fulfills the
    /// following requirements:
    /// - The start value is strictly less than the end value.
    /// - Both values are positive.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::locale;
    /// use icu::plurals::{
    ///     PluralCategory, PluralRuleType, PluralRulesOptions,
    ///     PluralRulesWithRanges,
    /// };
    ///
    /// let ranges = PluralRulesWithRanges::try_new(
    ///     locale!("sl").into(),
    ///     PluralRulesOptions::default().with_type(PluralRuleType::Ordinal),
    /// )
    /// .expect("locale should be present");
    ///
    /// assert_eq!(
    ///     ranges.resolve_range(PluralCategory::Other, PluralCategory::One),
    ///     PluralCategory::Few
    /// );
    /// ```
    ///
    /// [`Plural Category`]: PluralCategory
    pub fn resolve_range(&self, start: PluralCategory, end: PluralCategory) -> PluralCategory {
        self.ranges
            .get()
            .ranges
            .get_copied(&UnvalidatedPluralRange::from_range(
                start.into(),
                end.into(),
            ))
            .map(PluralCategory::from)
            .unwrap_or(end)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
/// A bag of values for different plural cases.
pub struct PluralElements<T>(PluralElementsInner<T>);

#[derive(Debug, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
pub(crate) struct PluralElementsInner<T> {
    #[cfg_attr(feature = "serde", serde(skip_serializing_if = "Option::is_none"))]
    zero: Option<T>,
    #[cfg_attr(feature = "serde", serde(skip_serializing_if = "Option::is_none"))]
    one: Option<T>,
    #[cfg_attr(feature = "serde", serde(skip_serializing_if = "Option::is_none"))]
    two: Option<T>,
    #[cfg_attr(feature = "serde", serde(skip_serializing_if = "Option::is_none"))]
    few: Option<T>,
    #[cfg_attr(feature = "serde", serde(skip_serializing_if = "Option::is_none"))]
    many: Option<T>,
    other: T,
    #[cfg_attr(feature = "serde", serde(skip_serializing_if = "Option::is_none"))]
    explicit_zero: Option<T>,
    #[cfg_attr(feature = "serde", serde(skip_serializing_if = "Option::is_none"))]
    explicit_one: Option<T>,
}

impl<T> PluralElements<T> {
    /// Creates a new [`PluralElements`] with the given default value.
    pub fn new(other: T) -> Self {
        Self(PluralElementsInner {
            other,
            zero: None,
            one: None,
            two: None,
            few: None,
            many: None,
            explicit_zero: None,
            explicit_one: None,
        })
    }

    /// The value for [`PluralCategory::Zero`]
    pub fn zero(&self) -> &T {
        self.0.zero.as_ref().unwrap_or(&self.0.other)
    }

    /// The value for [`PluralCategory::One`]
    pub fn one(&self) -> &T {
        self.0.one.as_ref().unwrap_or(&self.0.other)
    }

    /// The value for [`PluralCategory::Two`]
    pub fn two(&self) -> &T {
        self.0.two.as_ref().unwrap_or(&self.0.other)
    }

    /// The value for [`PluralCategory::Few`]
    pub fn few(&self) -> &T {
        self.0.few.as_ref().unwrap_or(&self.0.other)
    }

    /// The value for [`PluralCategory::Many`]
    pub fn many(&self) -> &T {
        self.0.many.as_ref().unwrap_or(&self.0.other)
    }

    /// The value for [`PluralCategory::Other`]
    pub fn other(&self) -> &T {
        &self.0.other
    }

    /// If the only variant is `other`, returns `Some(other)`.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_plurals::PluralElements;
    ///
    /// let mut only_other = PluralElements::new("abc").with_one_value(Some("abc"));
    /// assert_eq!(only_other.try_into_other(), Some("abc"));
    ///
    /// let mut multi = PluralElements::new("abc").with_one_value(Some("def"));
    /// assert_eq!(multi.try_into_other(), None);
    /// ```
    pub fn try_into_other(self) -> Option<T> {
        match self.0 {
            PluralElementsInner {
                zero: None,
                one: None,
                two: None,
                few: None,
                many: None,
                other,
                explicit_zero: None,
                explicit_one: None,
            } => Some(other),
            _ => None,
        }
    }

    /// The value used when the [`PluralOperands`] are exactly 0.
    pub fn explicit_zero(&self) -> Option<&T> {
        self.0.explicit_zero.as_ref()
    }

    /// The value used when the [`PluralOperands`] are exactly 1.
    pub fn explicit_one(&self) -> Option<&T> {
        self.0.explicit_one.as_ref()
    }

    /// Applies a function `f` to convert all values to another type.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_plurals::PluralElements;
    ///
    /// let x = PluralElements::new(11).with_one_value(Some(15));
    /// let y = x.map(|i| i * 2);
    ///
    /// assert_eq!(*y.other(), 22);
    /// assert_eq!(*y.one(), 30);
    /// ```
    pub fn map<B, F: FnMut(T) -> B>(self, mut f: F) -> PluralElements<B> {
        let Ok(x) = self.try_map(move |x| Ok::<B, Infallible>(f(x)));
        x
    }

    /// Applies a function `f` to convert all values to another type,
    /// propagating a possible error.
    pub fn try_map<B, E, F: FnMut(T) -> Result<B, E>>(
        self,
        mut f: F,
    ) -> Result<PluralElements<B>, E> {
        let plural_elements = PluralElements(PluralElementsInner {
            other: f(self.0.other)?,
            zero: self.0.zero.map(&mut f).transpose()?,
            one: self.0.one.map(&mut f).transpose()?,
            two: self.0.two.map(&mut f).transpose()?,
            few: self.0.few.map(&mut f).transpose()?,
            many: self.0.many.map(&mut f).transpose()?,
            explicit_zero: self.0.explicit_zero.map(&mut f).transpose()?,
            explicit_one: self.0.explicit_one.map(&mut f).transpose()?,
        });
        Ok(plural_elements)
    }

    /// Immutably applies a function `f` to each value.
    pub fn for_each<F: FnMut(&T)>(&self, mut f: F) {
        #[expect(clippy::unit_arg)] // consistency with map and one-liner
        let Ok(()) = self.try_for_each(move |x| Ok::<(), Infallible>(f(x)));
    }

    /// Immutably applies a function `f` to each value,
    /// propagating a possible error.
    pub fn try_for_each<E, F: FnMut(&T) -> Result<(), E>>(&self, mut f: F) -> Result<(), E> {
        // Use a structure to create compile errors if another field is added
        let _ = PluralElements(PluralElementsInner {
            other: f(&self.0.other)?,
            zero: self.0.zero.as_ref().map(&mut f).transpose()?,
            one: self.0.one.as_ref().map(&mut f).transpose()?,
            two: self.0.two.as_ref().map(&mut f).transpose()?,
            few: self.0.few.as_ref().map(&mut f).transpose()?,
            many: self.0.many.as_ref().map(&mut f).transpose()?,
            explicit_zero: self.0.explicit_zero.as_ref().map(&mut f).transpose()?,
            explicit_one: self.0.explicit_one.as_ref().map(&mut f).transpose()?,
        });
        Ok(())
    }

    /// Mutably applies a function `f` to each value.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_plurals::PluralElements;
    ///
    /// let mut x = PluralElements::new(11).with_one_value(Some(15));
    /// x.for_each_mut(|i| *i *= 2);
    ///
    /// assert_eq!(*x.other(), 22);
    /// assert_eq!(*x.one(), 30);
    /// ```
    pub fn for_each_mut<F: FnMut(&mut T)>(&mut self, mut f: F) {
        #[expect(clippy::unit_arg)] // consistency with map and one-liner
        let Ok(()) = self.try_for_each_mut(move |x| Ok::<(), Infallible>(f(x)));
    }

    /// Mutably applies a function `f` to each value,
    /// propagating a possible error.
    pub fn try_for_each_mut<E, F: FnMut(&mut T) -> Result<(), E>>(
        &mut self,
        mut f: F,
    ) -> Result<(), E> {
        // Use a structure to create compile errors if another field is added
        let _ = PluralElements(PluralElementsInner {
            other: f(&mut self.0.other)?,
            zero: self.0.zero.as_mut().map(&mut f).transpose()?,
            one: self.0.one.as_mut().map(&mut f).transpose()?,
            two: self.0.two.as_mut().map(&mut f).transpose()?,
            few: self.0.few.as_mut().map(&mut f).transpose()?,
            many: self.0.many.as_mut().map(&mut f).transpose()?,
            explicit_zero: self.0.explicit_zero.as_mut().map(&mut f).transpose()?,
            explicit_one: self.0.explicit_one.as_mut().map(&mut f).transpose()?,
        });
        Ok(())
    }

    /// Converts from `&PluralElements<T>` to `PluralElements<&T>`.
    pub fn as_ref(&self) -> PluralElements<&T> {
        PluralElements(PluralElementsInner {
            other: &self.0.other,
            zero: self.0.zero.as_ref(),
            one: self.0.one.as_ref(),
            two: self.0.two.as_ref(),
            few: self.0.few.as_ref(),
            many: self.0.many.as_ref(),
            explicit_zero: self.0.explicit_zero.as_ref(),
            explicit_one: self.0.explicit_one.as_ref(),
        })
    }
}

impl<T: PartialEq> PluralElements<T> {
    /// Sets the value for [`PluralCategory::Zero`].
    pub fn with_zero_value(self, zero: Option<T>) -> Self {
        Self(PluralElementsInner {
            zero: zero.filter(|t| *t != self.0.other),
            ..self.0
        })
    }

    /// Sets the value for [`PluralCategory::One`].
    pub fn with_one_value(self, one: Option<T>) -> Self {
        Self(PluralElementsInner {
            one: one.filter(|t| *t != self.0.other),
            ..self.0
        })
    }

    /// Sets the value for [`PluralCategory::Two`].
    pub fn with_two_value(self, two: Option<T>) -> Self {
        Self(PluralElementsInner {
            two: two.filter(|t| *t != self.0.other),
            ..self.0
        })
    }

    /// Sets the value for [`PluralCategory::Few`].
    pub fn with_few_value(self, few: Option<T>) -> Self {
        Self(PluralElementsInner {
            few: few.filter(|t| *t != self.0.other),
            ..self.0
        })
    }

    /// Sets the value for [`PluralCategory::Many`].
    pub fn with_many_value(self, many: Option<T>) -> Self {
        Self(PluralElementsInner {
            many: many.filter(|t| *t != self.0.other),
            ..self.0
        })
    }

    /// Sets the value for explicit 0.
    pub fn with_explicit_zero_value(self, explicit_zero: Option<T>) -> Self {
        Self(PluralElementsInner {
            explicit_zero,
            ..self.0
        })
    }

    /// Sets the value for explicit 1.
    pub fn with_explicit_one_value(self, explicit_one: Option<T>) -> Self {
        Self(PluralElementsInner {
            explicit_one,
            ..self.0
        })
    }
}
