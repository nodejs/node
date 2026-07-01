// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::Decimal;
use icu_decimal::options::DecimalFormatterOptions;
use icu_decimal::{DecimalFormatter, DecimalFormatterPreferences};
use icu_locale_core::preferences::{define_preferences, prefs_convert};

use icu_provider::prelude::*;

use super::super::provider::percent::PercentEssentialsV1;
use super::format::FormattedPercent;
use super::options::PercentFormatterOptions;

extern crate alloc;

define_preferences!(
    /// The preferences for percent formatting.
    [Copy]
    PercentFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: crate::dimension::preferences::NumberingSystem
    }
);

prefs_convert!(PercentFormatterPreferences, DecimalFormatterPreferences, {
    numbering_system
});

/// A formatter for percent values.
///
/// [`PercentFormatter`] supports:
///   1. Rendering in the locale's percent system.
#[derive(Debug)]
pub struct PercentFormatter<R> {
    /// Essential data for the percent formatter.
    essential: DataPayload<PercentEssentialsV1>,

    /// Options bag for the percent formatter to determine the behavior of the formatter.
    options: PercentFormatterOptions,

    /// A fixed decimal formatter used to format the percent value.
    decimal_formatter: R,
}

impl PercentFormatter<DecimalFormatter> {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: PercentFormatterPreferences, options: PercentFormatterOptions) -> error: DataError,
        functions: [
            try_new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    /// Creates a new [`PercentFormatter`] from compiled locale data and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: PercentFormatterPreferences,
        options: PercentFormatterOptions,
    ) -> Result<Self, DataError> {
        let decimal_formatter =
            DecimalFormatter::try_new((&prefs).into(), DecimalFormatterOptions::default())?;

        PercentFormatter::try_new_with_decimal_formatter(prefs, decimal_formatter, options)
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D>(
        provider: &D,
        prefs: PercentFormatterPreferences,
        options: PercentFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<PercentEssentialsV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>,
    {
        let decimal_formatter = DecimalFormatter::try_new_unstable(
            provider,
            (&prefs).into(),
            DecimalFormatterOptions::default(),
        )?;

        PercentFormatter::try_new_with_decimal_formatter_unstable(
            provider,
            prefs,
            decimal_formatter,
            options,
        )
    }

    /// Formats a [`Decimal`] value for the given percent code.
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::dimension::percent::formatter::PercentFormatter;
    /// use icu::locale::locale;
    /// use writeable::Writeable;
    ///
    /// let locale = locale!("en-US").into();
    /// let fmt = PercentFormatter::try_new(locale, Default::default()).unwrap();
    /// let value = "12345.67".parse().unwrap();
    /// let formatted_percent = fmt.format(&value);
    /// let mut sink = String::new();
    /// formatted_percent.write_to(&mut sink).unwrap();
    /// assert_eq!(sink.as_str(), "12,345.67%");
    /// ```
    pub fn format<'l>(&'l self, value: &'l Decimal) -> FormattedPercent<'l> {
        FormattedPercent {
            value,
            essential: self.essential.get(),
            decimal_formatter: &self.decimal_formatter,
            options: &self.options,
        }
    }
}

impl<R> PercentFormatter<R>
where
    R: AsRef<DecimalFormatter>,
{
    /// Creates a new [`PercentFormatter`] from compiled locale data, an options bag and fixed decimal formatter.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_with_decimal_formatter(
        prefs: PercentFormatterPreferences,
        decimal_formatter: R,
        options: PercentFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = PercentEssentialsV1::make_locale(prefs.locale_preferences);
        let essential = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            essential,
            options,
            decimal_formatter,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_with_decimal_formatter_unstable(
        provider: &(impl DataProvider<PercentEssentialsV1> + ?Sized),
        prefs: PercentFormatterPreferences,
        decimal_formatter: R,
        options: PercentFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = PercentEssentialsV1::make_locale(prefs.locale_preferences);
        let essential = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            essential,
            options,
            decimal_formatter,
        })
    }
}
