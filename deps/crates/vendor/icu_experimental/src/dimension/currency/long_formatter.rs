// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::fmt::Display;

use fixed_decimal::Decimal;
use icu_decimal::{options::DecimalFormatterOptions, DecimalFormatter};
use icu_plurals::PluralRules;
use icu_provider::prelude::*;
use writeable::Writeable;

use crate::dimension::provider::currency::{
    extended::CurrencyExtendedDataV1, patterns::CurrencyPatternsDataV1,
};

use super::{formatter::CurrencyFormatterPreferences, CurrencyCode};

extern crate alloc;

/// A formatter for monetary values.
///
/// [`LongCurrencyFormatter`] supports:
///   1. Rendering in the locale's currency system.
///   2. Locale-sensitive grouping separator positions.
///
/// Read more about the options in the [`super::options`] module.
#[derive(Debug)]
pub struct LongCurrencyFormatter {
    /// Extended data for the currency formatter.
    extended: DataPayload<CurrencyExtendedDataV1>,

    /// Formatting patterns for each currency plural category.
    patterns: DataPayload<CurrencyPatternsDataV1>,

    /// A [`DecimalFormatter`] to format the currency value.
    decimal_formatter: DecimalFormatter,

    /// A [`PluralRules`] to determine the plural category of the unit.
    plural_rules: PluralRules,
}

impl LongCurrencyFormatter {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: CurrencyFormatterPreferences, currency_code: &CurrencyCode) -> error: DataError,
        functions: [
            try_new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    /// Creates a new [`LongCurrencyFormatter`] from compiled locale data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: CurrencyFormatterPreferences,
        currency_code: &CurrencyCode,
    ) -> Result<Self, DataError> {
        let locale = CurrencyPatternsDataV1::make_locale(prefs.locale_preferences);
        let decimal_formatter =
            DecimalFormatter::try_new((&prefs).into(), DecimalFormatterOptions::default())?;

        let marker_attributes = DataMarkerAttributes::try_from_str(currency_code.0.as_str())
            .map_err(|_| {
                DataErrorKind::IdentifierNotFound
                    .into_error()
                    .with_debug_context("failed to get data marker attribute from a `CurrencyCode`")
            })?;

        let extended = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    marker_attributes,
                    &locale,
                ),
                ..Default::default()
            })?
            .payload;

        let patterns = crate::provider::Baked.load(Default::default())?.payload;

        let plural_rules = PluralRules::try_new_cardinal((&prefs).into())?;

        Ok(Self {
            extended,
            patterns,
            decimal_formatter,
            plural_rules,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D>(
        provider: &D,
        prefs: CurrencyFormatterPreferences,
        currency_code: &CurrencyCode,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<CurrencyExtendedDataV1>
            + DataProvider<CurrencyPatternsDataV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        let locale = CurrencyPatternsDataV1::make_locale(prefs.locale_preferences);
        let decimal_formatter = DecimalFormatter::try_new_unstable(
            provider,
            (&prefs).into(),
            DecimalFormatterOptions::default(),
        )?;

        let marker_attributes = DataMarkerAttributes::try_from_str(currency_code.0.as_str())
            .map_err(|_| {
                DataErrorKind::IdentifierNotFound
                    .into_error()
                    .with_debug_context("failed to get data marker attribute from a `CurrencyCode`")
            })?;
        let extended = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    marker_attributes,
                    &locale,
                ),
                ..Default::default()
            })?
            .payload;

        let patterns = provider.load(Default::default())?.payload;

        let plural_rules = PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?;

        Ok(Self {
            extended,
            patterns,
            decimal_formatter,
            plural_rules,
        })
    }

    /// Formats in the long format a [`Decimal`] value for the given currency code.
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::dimension::currency::long_formatter::LongCurrencyFormatter;
    /// use icu::experimental::dimension::currency::CurrencyCode;
    /// use icu::locale::locale;
    /// use tinystr::*;
    /// use writeable::assert_writeable_eq;
    ///
    /// let currency_preferences = locale!("en-US").into();
    /// let currency_code = CurrencyCode(tinystr!(3, "USD"));
    /// let fmt = LongCurrencyFormatter::try_new(currency_preferences, &currency_code).unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// assert_writeable_eq!(fmt.format_fixed_decimal(&value), "12,345.67 US dollars");
    /// ```
    pub fn format_fixed_decimal<'l>(&'l self, value: &'l Decimal) -> impl Writeable + Display + 'l {
        let operands = value.into();
        let display_name = self
            .extended
            .get()
            .display_names
            .get(operands, &self.plural_rules);
        let pattern = self
            .patterns
            .get()
            .patterns
            .get(operands, &self.plural_rules);

        self.decimal_formatter.format_sign(
            value.sign,
            pattern.interpolate((
                self.decimal_formatter
                    .format_unsigned(icu_decimal::Cow::Borrowed(&value.absolute)),
                display_name,
            )),
        )
    }
}
