// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::fmt::Display;

use crate::dimension::provider::{
    currency::compact::ShortCurrencyCompactV1, currency::essentials::CurrencyEssentialsV1,
};
use fixed_decimal::Decimal;
use icu_decimal::preferences::CompactDecimalFormatterPreferences;
use icu_decimal::{DecimalFormatter, DecimalFormatterPreferences};
use icu_locale_core::preferences::{define_preferences, prefs_convert};
use icu_plurals::{PluralRules, PluralRulesPreferences};
use icu_provider::prelude::*;
use writeable::Writeable;

use super::{options::CurrencyFormatterOptions, CurrencyCode};

extern crate alloc;

define_preferences!(
    /// The preferences for currency formatting.
    [Copy]
    CompactCurrencyFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: crate::dimension::preferences::NumberingSystem
    }
);

prefs_convert!(
    CompactCurrencyFormatterPreferences,
    DecimalFormatterPreferences,
    { numbering_system }
);
prefs_convert!(
    CompactCurrencyFormatterPreferences,
    CompactDecimalFormatterPreferences
);
prefs_convert!(CompactCurrencyFormatterPreferences, PluralRulesPreferences);

/// A formatter for monetary values.
///
/// [`CompactCurrencyFormatter`] supports:
///   1. Rendering in the locale's currency system.
///   2. Locale-sensitive grouping separator positions.
///
/// Read more about the options in the [`super::options`] module.
#[derive(Debug)]
pub struct CompactCurrencyFormatter {
    /// Short currency compact data for the compact currency formatter.
    _short_currency_compact: DataPayload<ShortCurrencyCompactV1>,

    /// Essential data for the compact currency formatter.
    essential: DataPayload<CurrencyEssentialsV1>,

    decimal_formatter: DecimalFormatter,

    compact_data: DataPayload<icu_decimal::provider::DecimalCompactShortV1>,

    plural_rules: PluralRules,

    /// Options bag for the compact currency formatter to determine the behavior of the formatter.
    /// for example: width.
    options: CurrencyFormatterOptions,
}

impl CompactCurrencyFormatter {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: CompactCurrencyFormatterPreferences, options: CurrencyFormatterOptions) -> error: DataError,
        functions: [
            try_new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    /// Creates a new [`CompactCurrencyFormatter`] from compiled locale data and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: CompactCurrencyFormatterPreferences,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError> {
        let short_locale = ShortCurrencyCompactV1::make_locale(prefs.locale_preferences);

        let short_currency_compact = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&short_locale),
                ..Default::default()
            })?
            .payload;

        let essential_locale = CurrencyEssentialsV1::make_locale(prefs.locale_preferences);

        let essential = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&essential_locale),
                ..Default::default()
            })?
            .payload;

        let decimal_formatter = DecimalFormatter::try_new((&prefs).into(), Default::default())?;

        let compact_data = DataProvider::<icu_decimal::provider::DecimalCompactShortV1>::load(
            &icu_decimal::provider::Baked,
            DataRequest {
                id: DataIdentifierBorrowed::for_locale(
                    &icu_decimal::provider::DecimalCompactShortV1::make_locale(
                        prefs.locale_preferences,
                    ),
                ),
                ..Default::default()
            },
        )?
        .payload
        .cast();

        let plural_rules = PluralRules::try_new_cardinal((&prefs).into())?;

        Ok(Self {
            _short_currency_compact: short_currency_compact,
            essential,
            decimal_formatter,
            compact_data,
            plural_rules,
            options,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D>(
        provider: &D,
        prefs: CompactCurrencyFormatterPreferences,
        options: CurrencyFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyEssentialsV1>
            + DataProvider<ShortCurrencyCompactV1>
            + DataProvider<icu_decimal::provider::DecimalCompactShortV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        let locale = CurrencyEssentialsV1::make_locale(prefs.locale_preferences);

        let decimal_formatter =
            DecimalFormatter::try_new_unstable(provider, (&prefs).into(), Default::default())?;

        let compact_data = DataProvider::<icu_decimal::provider::DecimalCompactShortV1>::load(
            provider,
            DataRequest {
                id: DataIdentifierBorrowed::for_locale(
                    &icu_decimal::provider::DecimalCompactShortV1::make_locale(
                        prefs.locale_preferences,
                    ),
                ),
                ..Default::default()
            },
        )?
        .payload
        .cast();

        let plural_rules = PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?;

        let short_currency_compact = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        let essential = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            _short_currency_compact: short_currency_compact,
            essential,
            decimal_formatter,
            compact_data,
            plural_rules,
            options,
        })
    }

    /// Formats in the compact format a [`Decimal`] value for the given currency code.
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::dimension::currency::compact_formatter::CompactCurrencyFormatter;
    /// use icu::experimental::dimension::currency::CurrencyCode;
    /// use icu::locale::locale;
    /// use tinystr::*;
    /// use writeable::assert_writeable_eq;
    ///
    /// let locale = locale!("en-US").into();
    /// let currency_code = CurrencyCode(tinystr!(3, "USD"));
    /// let fmt = CompactCurrencyFormatter::try_new(locale, Default::default()).unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// assert_writeable_eq!(fmt.format_fixed_decimal(&value, &currency_code), "$12K");
    /// ```
    pub fn format_fixed_decimal<'l>(
        &'l self,
        value: &'l Decimal,
        currency_code: &'l CurrencyCode,
    ) -> impl Writeable + Display + 'l {
        let (currency_placeholder, pattern, _pattern_selection) = self
            .essential
            .get()
            .name_and_pattern(self.options.width, currency_code);

        // TODO: The current behavior is the behavior when there is no compact currency pattern found.
        // Therefore, in the next PR, we will add the code to handle using the compact currency patterns.

        let (compact_pattern, significand) = self
            .compact_data
            .get()
            .get_pattern_and_significand(&value.absolute, &self.plural_rules);

        self.decimal_formatter.format_sign(
            value.sign,
            pattern.interpolate((
                compact_pattern
                    .unwrap_or(icu_pattern::SinglePlaceholderPattern::PASS_THROUGH)
                    .interpolate([self
                        .decimal_formatter
                        .format_unsigned(icu_decimal::Cow::Owned(significand))]),
                currency_placeholder,
            )),
        )
    }
}
