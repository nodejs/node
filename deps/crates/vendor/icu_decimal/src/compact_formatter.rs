// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::fmt::Display;

use crate::input::Decimal;
use crate::Cow;
use crate::{
    error::CompactExponentError,
    options::CompactDecimalFormatterOptions,
    preferences::{CompactDecimalFormatterPreferences, DecimalFormatterPreferences},
    provider::*,
    DecimalFormatter,
};
#[cfg(feature = "alloc")]
use alloc::string::String;
use fixed_decimal::UnsignedDecimal;
use icu_pattern::{Pattern, PatternBackend, SinglePlaceholder};
use icu_plurals::PluralRules;
use icu_provider::DataError;
use icu_provider::{marker::ErasedMarker, prelude::*};
use writeable::Writeable;

/// A formatter that renders locale-sensitive compact numbers.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
///
/// Graduation tracking issue: [issue #7161](https://github.com/unicode-org/icu4x/issues/7161).
/// </div>
///
/// ✨ *Enabled with the `unstable` Cargo feature.*
///
/// # Examples
///
/// ```
/// use icu::decimal::CompactDecimalFormatter;
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let short_french = CompactDecimalFormatter::try_new_short(
///     locale!("fr").into(),
///     Default::default(),
/// )
/// .unwrap();
///
/// let [long_french, long_japanese, long_bangla] =
///     [locale!("fr"), locale!("ja"), locale!("bn")].map(|locale| {
///         CompactDecimalFormatter::try_new_long(
///             locale.into(),
///             Default::default(),
///         )
///         .unwrap()
///     });
///
/// /// Supports short and long notations:
/// # // The following line contains U+00A0 NO-BREAK SPACE.
/// assert_writeable_eq!(short_french.format(&35_357_670i64.into()), "35 M");
/// assert_writeable_eq!(
///     long_french.format(&35_357_670i64.into()),
///     "35 millions"
/// );
/// /// The powers of ten used are locale-dependent:
/// assert_writeable_eq!(long_japanese.format(&3535_7670i64.into()), "3536万");
/// /// So are the digits:
/// assert_writeable_eq!(
///     long_bangla.format(&3_53_57_670i64.into()),
///     "৩.৫ কোটি"
/// );
///
/// /// The output does not always contain digits:
/// assert_writeable_eq!(long_french.format(&1000i64.into()), "mille");
/// ```
#[derive(Debug)]
pub struct CompactDecimalFormatter {
    plural_rules: PluralRules,
    decimal_formatter: DecimalFormatter,
    compact_data:
        DataPayload<ErasedMarker<<DecimalCompactLongV1 as DynamicDataMarker>::DataStruct>>,
}

impl CompactDecimalFormatter {
    /// Creates a new short [`CompactDecimalFormatter`] from compiled data and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::decimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let formatter = CompactDecimalFormatter::try_new_short(
    ///     locale!("sv").into(),
    ///     Default::default(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(formatter.format(&1234.into()), "1,2 tn");
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_new_short(
        prefs: CompactDecimalFormatterPreferences,
        options: CompactDecimalFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = DecimalCompactShortV1::make_locale(prefs.locale_preferences);
        Ok(Self {
            decimal_formatter: DecimalFormatter::try_new((&prefs).into(), options.into())?,
            plural_rules: PluralRules::try_new_cardinal((&prefs).into())?,
            compact_data: load_with_fallback::<DecimalCompactShortV1>(
                &Baked,
                DecimalFormatterPreferences::from(&prefs)
                    .nu_id(&locale)
                    .into_iter()
                    .chain([DataIdentifierBorrowed::for_locale(&locale)]),
            )?
            .payload
            .cast(),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CompactDecimalFormatterPreferences, options: CompactDecimalFormatterOptions) -> error: DataError,
        functions: [
            try_new_short: skip,
            try_new_short_with_buffer_provider,
            try_new_short_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short)]
    pub fn try_new_short_unstable<D>(
        provider: &D,
        prefs: CompactDecimalFormatterPreferences,
        options: CompactDecimalFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<DecimalCompactShortV1>
            + DataProvider<DecimalSymbolsV1>
            + DataProvider<DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>
            + ?Sized,
    {
        let locale = DecimalCompactShortV1::make_locale(prefs.locale_preferences);
        Ok(Self {
            decimal_formatter: DecimalFormatter::try_new_unstable(
                provider,
                (&prefs).into(),
                options.into(),
            )?,
            plural_rules: PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?,
            compact_data: load_with_fallback::<DecimalCompactShortV1>(
                provider,
                DecimalFormatterPreferences::from(&prefs)
                    .nu_id(&locale)
                    .into_iter()
                    .chain([DataIdentifierBorrowed::for_locale(&locale)]),
            )?
            .payload
            .cast(),
        })
    }

    /// Creates a new long [`CompactDecimalFormatter`] from compiled data and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::decimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let formatter = CompactDecimalFormatter::try_new_long(
    ///     locale!("sv").into(),
    ///     Default::default(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(formatter.format(&1234.into()), "1,2 tusen");
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_new_long(
        prefs: CompactDecimalFormatterPreferences,
        options: CompactDecimalFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = DecimalCompactLongV1::make_locale(prefs.locale_preferences);
        Ok(Self {
            decimal_formatter: DecimalFormatter::try_new((&prefs).into(), options.into())?,
            plural_rules: PluralRules::try_new_cardinal((&prefs).into())?,
            compact_data: load_with_fallback::<DecimalCompactLongV1>(
                &Baked,
                DecimalFormatterPreferences::from(&prefs)
                    .nu_id(&locale)
                    .into_iter()
                    .chain([DataIdentifierBorrowed::for_locale(&locale)]),
            )?
            .payload
            .cast(),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: CompactDecimalFormatterPreferences, options: CompactDecimalFormatterOptions) -> error: DataError,
        functions: [
            try_new_long: skip,
            try_new_long_with_buffer_provider,
            try_new_long_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_long)]
    pub fn try_new_long_unstable<D>(
        provider: &D,
        prefs: CompactDecimalFormatterPreferences,
        options: CompactDecimalFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<DecimalCompactLongV1>
            + DataProvider<DecimalSymbolsV1>
            + DataProvider<DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>
            + ?Sized,
    {
        let locale = DecimalCompactLongV1::make_locale(prefs.locale_preferences);
        Ok(Self {
            decimal_formatter: DecimalFormatter::try_new_unstable(
                provider,
                (&prefs).into(),
                options.into(),
            )?,
            plural_rules: PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?,
            compact_data: load_with_fallback::<DecimalCompactLongV1>(
                provider,
                DecimalFormatterPreferences::from(&prefs)
                    .nu_id(&locale)
                    .into_iter()
                    .chain([DataIdentifierBorrowed::for_locale(&locale)]),
            )?
            .payload
            .cast(),
        })
    }

    /// Formats a [`Decimal`] by automatically scaling and rounding it.
    ///
    /// The result may have a fractional digit only if it is compact and its
    /// significand is less than 10. Trailing fractional 0s are omitted.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::decimal::input::{Decimal, SignDisplay};
    /// use icu::decimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    /// use writeable::assert_writeable_eq;
    ///
    /// let short_english = CompactDecimalFormatter::try_new_short(
    ///     locale!("en").into(),
    ///     Default::default(),
    /// )
    /// .unwrap();
    ///
    /// assert_writeable_eq!(short_english.format(&Decimal::from(0)), "0");
    /// assert_writeable_eq!(short_english.format(&Decimal::from(2)), "2");
    /// assert_writeable_eq!(short_english.format(&Decimal::from(843)), "843");
    /// assert_writeable_eq!(short_english.format(&Decimal::from(2207)), "2.2K");
    /// assert_writeable_eq!(short_english.format(&Decimal::from(15127)), "15K");
    /// assert_writeable_eq!(short_english.format(&Decimal::from(3010349)), "3M");
    /// assert_writeable_eq!(short_english.format(&Decimal::from(-13132)), "-13K");
    ///
    /// // The sign display on the Decimal is respected:
    /// assert_writeable_eq!(
    ///     short_english.format(
    ///         &Decimal::from(2500).with_sign_display(SignDisplay::ExceptZero)
    ///     ),
    ///     "+2.5K"
    /// );
    /// ```
    ///
    /// The result is the nearest such compact number, with halfway cases-
    /// rounded towards the number with an even least significant digit.
    ///
    /// ```
    /// # use icu::decimal::CompactDecimalFormatter;
    /// # use icu::locale::locale;
    /// # use writeable::assert_writeable_eq;
    /// #
    /// # let short_english = CompactDecimalFormatter::try_new_short(
    /// #    locale!("en").into(),
    /// #    Default::default(),
    /// # ).unwrap();
    /// assert_writeable_eq!(
    ///     short_english.format(&"999499.99".parse().unwrap()),
    ///     "999K"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format(&"999500.00".parse().unwrap()),
    ///     "1M"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format(&"1650".parse().unwrap()),
    ///     "1.6K"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format(&"1750".parse().unwrap()),
    ///     "1.8K"
    /// );
    /// assert_writeable_eq!(short_english.format(&"1950".parse().unwrap()), "2K");
    /// assert_writeable_eq!(
    ///     short_english.format(&"-1172700".parse().unwrap()),
    ///     "-1.2M"
    /// );
    /// assert_writeable_eq!(
    ///     short_english.format(&"0.2222".parse().unwrap()),
    ///     "0.22"
    /// );
    /// ```
    ///
    /// Floating point inputs should use [`FloatPrecision::RoundTrip`](fixed_decimal::FloatPrecision::RoundTrip).
    ///
    /// ```
    /// # use icu::decimal::input::{Decimal, FloatPrecision};
    /// # use icu::decimal::CompactDecimalFormatter;
    /// # use icu::locale::locale;
    /// # use writeable::assert_writeable_eq;
    /// #
    /// # let short_english = CompactDecimalFormatter::try_new_short(
    /// #    locale!("en").into(),
    /// #    Default::default(),
    /// # ).unwrap();
    /// assert_writeable_eq!(
    ///     short_english.format(
    ///         &Decimal::try_from_f64(999_499.99, FloatPrecision::RoundTrip)
    ///             .unwrap()
    ///     ),
    ///     "999K"
    /// );
    /// ```
    pub fn format(&self, value: &Decimal) -> impl Writeable + Display + '_ {
        let (compact_pattern, significand) = self
            .compact_data
            .get()
            .get_pattern_and_significand(&value.absolute, &self.plural_rules);

        self.decimal_formatter.format_sign(
            value.sign,
            compact_pattern
                .unwrap_or(Pattern::<SinglePlaceholder>::PASS_THROUGH)
                .interpolate([self
                    .decimal_formatter
                    .format_unsigned(Cow::Owned(significand))]),
        )
    }

    /// Formats a [`Decimal`], returning a [`String`].
    ///
    /// ✨ *Enabled with the `alloc` Cargo feature.*
    #[cfg(feature = "alloc")]
    pub fn format_to_string(&self, value: &Decimal) -> String {
        use writeable::Writeable;
        self.format(value).write_to_string().into_owned()
    }

    /// Formats a [`Decimal`] with a given exponent according to locale data.
    ///
    /// This is an advanced API; prefer using [`Self::format()`] in simple
    /// cases.
    ///
    /// Since the caller specifies the exact digits that are displayed, this
    /// allows for arbitrarily complex rounding rules.
    /// However, contrary to [`DecimalFormatter::format()`], this operation
    /// can fail, because the given exponent can be inconsistent with
    /// the locale data; for instance, if the locale uses lakhs and crores and
    /// millions are requested, or vice versa, this function returns an error.
    ///
    /// The given exponent should be constructed using
    /// [`Self::compact_exponent_for_magnitude()`] on the same
    /// [`CompactDecimalFormatter`] object.
    /// Specifically, `formatter.format_with_exponent(s, e)` requires that `e`
    /// be equal to `formatter.compact_exponent_for_magnitude(s.nonzero_magnitude_start() + e)`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use icu::decimal::{CompactDecimalFormatter, input::Decimal};
    /// # use icu::locale::locale;
    /// # use writeable::assert_writeable_eq;
    /// # use std::str::FromStr;
    /// #
    /// # let short_french = CompactDecimalFormatter::try_new_short(
    /// #    locale!("fr").into(),
    /// #    Default::default(),
    /// # ).unwrap();
    /// # let long_french = CompactDecimalFormatter::try_new_long(
    /// #    locale!("fr").into(),
    /// #    Default::default()
    /// # ).unwrap();
    /// # let long_bangla = CompactDecimalFormatter::try_new_long(
    /// #    locale!("bn").into(),
    /// #    Default::default()
    /// # ).unwrap();
    /// #
    /// let one_point_two = Decimal::from_str("1.20").unwrap();
    /// let three = Decimal::from_str("+3").unwrap();
    /// let ten = 10.into();
    /// # // The following line contains U+00A0 NO-BREAK SPACE.
    /// assert_writeable_eq!(
    ///     short_french
    ///         .format_with_exponent(&one_point_two, 6)
    ///         .unwrap(),
    ///     "1,20 M"
    /// );
    /// assert_writeable_eq!(
    ///     long_french.format_with_exponent(&one_point_two, 6).unwrap(),
    ///     "1,20 million"
    /// );
    ///
    /// # // The following line contains U+00A0 NO-BREAK SPACE.
    /// assert_writeable_eq!(
    ///     short_french.format_with_exponent(&three, 6).unwrap(),
    ///     "+3 M"
    /// );
    /// assert_writeable_eq!(
    ///     long_french.format_with_exponent(&three, 6).unwrap(),
    ///     "+3 millions"
    /// );
    ///
    /// assert_writeable_eq!(
    ///     long_bangla.format_with_exponent(&ten, 5).unwrap(),
    ///     "১০ লাখ"
    /// );
    ///
    /// assert_eq!(
    ///     long_bangla
    ///         .format_with_exponent(&one_point_two, 6)
    ///         .err()
    ///         .unwrap()
    ///         .to_string(),
    ///     "Expected compact exponent 5 for 10^6, got 6",
    /// );
    /// assert_eq!(
    ///     long_french
    ///         .format_with_exponent(&ten, 5)
    ///         .err()
    ///         .unwrap()
    ///         .to_string(),
    ///     "Expected compact exponent 6 for 10^6, got 5",
    /// );
    ///
    /// /// Some patterns omit the digits; in those cases, the output does not
    /// /// contain the sequence of digits specified by the significand.
    /// let one = 1.into();
    /// assert_writeable_eq!(
    ///     long_french.format_with_exponent(&one, 3).unwrap(),
    ///     "mille"
    /// );
    /// ```
    pub fn format_with_exponent<'l>(
        &'l self,
        significand: &'l Decimal,
        exponent: u8,
    ) -> Result<impl Writeable + Display + 'l, CompactExponentError> {
        let log10_type = significand.absolute.nonzero_magnitude_start() + i16::from(exponent);

        let (pattern, expected_exponent) = self
            .compact_data
            .get()
            .0
            .iter()
            .filter(|&t| log10_type >= i16::from(t.sized))
            .last()
            .map(|t| {
                (
                    t.variable
                        .get((&significand.absolute).into(), &self.plural_rules)
                        .1,
                    t.sized - t.variable.get_default().0.get(),
                )
            })
            .unwrap_or((Pattern::<SinglePlaceholder>::PASS_THROUGH, 0));

        if exponent != expected_exponent {
            return Err(CompactExponentError {
                actual: exponent,
                expected: expected_exponent,
                magnitude: log10_type,
            });
        }

        Ok(self.decimal_formatter.format_sign(
            significand.sign,
            pattern.interpolate([self
                .decimal_formatter
                .format_unsigned(Cow::Borrowed(&significand.absolute))]),
        ))
    }

    /// Returns the compact decimal exponent that should be used for a number of
    /// the given magnitude when using this formatter.
    ///
    /// # Examples
    /// ```
    /// use icu::decimal::CompactDecimalFormatter;
    /// use icu::locale::locale;
    ///
    /// let [long_french, long_japanese, long_bangla] = [
    ///     locale!("fr").into(),
    ///     locale!("ja").into(),
    ///     locale!("bn").into(),
    /// ]
    /// .map(|locale| {
    ///     CompactDecimalFormatter::try_new_long(locale, Default::default())
    ///         .unwrap()
    /// });
    /// /// French uses millions.
    /// assert_eq!(long_french.compact_exponent_for_magnitude(6), 6);
    /// /// Bangla uses lakhs.
    /// assert_eq!(long_bangla.compact_exponent_for_magnitude(6), 5);
    /// /// Japanese uses myriads.
    /// assert_eq!(long_japanese.compact_exponent_for_magnitude(6), 4);
    /// ```
    pub fn compact_exponent_for_magnitude(&self, magnitude: i16) -> u8 {
        self.compact_data
            .get()
            .0
            .iter()
            .filter(|t| magnitude >= i16::from(t.sized))
            .last()
            .map(|t| t.sized - t.variable.get_default().0.get())
            .unwrap_or_default()
    }
}

impl<'a, P: PatternBackend> CompactPatterns<'a, P> {
    /// Gets the compact pattern and significand for the given decimal
    pub fn get_pattern_and_significand(
        &'a self,
        value: &UnsignedDecimal,
        rules: &PluralRules,
    ) -> (Option<&'a Pattern<P>>, UnsignedDecimal) {
        let log10_type = value.nonzero_magnitude_start();

        let entry = self
            .0
            .iter()
            .enumerate()
            .filter(|&(_, t)| i16::from(t.sized) <= log10_type)
            .last();

        let exponent = entry
            .map(|(_, t)| t.sized - t.variable.get_default().0.get())
            .unwrap_or_default();

        let rounding_magnitude = if log10_type > i16::from(exponent) {
            // If we have at least 2 digits before the decimal point,
            // round to eliminate the fractional part.
            i16::from(exponent)
        } else {
            // …otherwise, round to two significant digits
            log10_type - 1
        };

        if let Some(t) = self
            .0
            .get(entry.map(|(idx, _)| idx + 1).unwrap_or_default())
        {
            let next_exponent = t.sized - t.variable.get_default().0.get();

            let rounds_to_next_exponent = log10_type + 1 == i16::from(next_exponent)
                && value.digit_at(rounding_magnitude - 1) >= 5
                && (rounding_magnitude..=log10_type).all(|m| value.digit_at(m) == 9);

            // We got bumped up a magnitude by rounding.
            if rounds_to_next_exponent {
                return (
                    Some(t.variable.get(1.into(), rules).1),
                    UnsignedDecimal::ONE,
                );
            }
        }

        let significand = value
            .clone()
            .rounded(rounding_magnitude)
            .multiplied_pow10(-i16::from(exponent))
            .trimmed_end();

        (
            entry.map(|(_, t)| t.variable.get((&significand).into(), rules).1),
            significand,
        )
    }
}

#[cfg(feature = "serde")]
#[cfg(test)]
mod tests {
    use super::*;
    use crate::options::GroupingStrategy;
    use icu_locale_core::locale;
    use writeable::assert_writeable_eq;

    #[allow(non_snake_case)]
    #[test]
    fn test_grouping() {
        // https://unicode-org.atlassian.net/browse/ICU-22254
        #[derive(Debug)]
        struct TestCase<'a> {
            short: bool,
            options: CompactDecimalFormatterOptions,
            expected1T: &'a str,
            expected10T: &'a str,
        }
        let cases = [
            TestCase {
                short: true,
                options: Default::default(),
                expected1T: "1000T",
                expected10T: "10,000T",
            },
            TestCase {
                short: true,
                options: GroupingStrategy::Always.into(),
                expected1T: "1,000T",
                expected10T: "10,000T",
            },
            TestCase {
                short: true,
                options: GroupingStrategy::Never.into(),
                expected1T: "1000T",
                expected10T: "10000T",
            },
            TestCase {
                short: false,
                options: Default::default(),
                expected1T: "1000 trillion",
                expected10T: "10,000 trillion",
            },
            TestCase {
                short: false,
                options: GroupingStrategy::Always.into(),
                expected1T: "1,000 trillion",
                expected10T: "10,000 trillion",
            },
            TestCase {
                short: false,
                options: GroupingStrategy::Never.into(),
                expected1T: "1000 trillion",
                expected10T: "10000 trillion",
            },
        ];
        for case in cases {
            let formatter = if case.short {
                CompactDecimalFormatter::try_new_short(locale!("en").into(), case.options.clone())
            } else {
                CompactDecimalFormatter::try_new_long(locale!("en").into(), case.options.clone())
            }
            .unwrap();
            let result1T = formatter.format(&1_000_000_000_000_000i64.into());
            assert_writeable_eq!(result1T, case.expected1T, "{:?}", case);
            let result10T = formatter.format(&10_000_000_000_000_000i64.into());
            assert_writeable_eq!(result10T, case.expected10T, "{:?}", case);
        }
    }

    #[test]
    fn regression_7387() {
        let formatter =
            CompactDecimalFormatter::try_new_short(locale!("ar").into(), Default::default())
                .unwrap();

        assert_writeable_eq!(formatter.format(&3_000_000i64.into()), "3\u{a0}مليون");
    }

    #[test]
    fn numbering_system() {
        let default =
            CompactDecimalFormatter::try_new_short(locale!("lo").into(), Default::default())
                .unwrap();
        let lao = CompactDecimalFormatter::try_new_short(
            locale!("lo-u-nu-laoo").into(),
            Default::default(),
        )
        .unwrap();

        // lo is one of the few locales that have different compact patterns per
        // numbering system (they differ by a NBSP)
        assert_writeable_eq!(default.format(&12345.into()), "12 ພັນ");
        assert_writeable_eq!(lao.format(&12345.into()), "໑໒ພັນ");
    }
}
