// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::dimension::units::format::FormattedUnit;
use crate::dimension::units::options::Width;
use crate::measure::category::CategorizedMeasureUnit;
use crate::measure::category::MeasureUnitCategory;
use core::marker::PhantomData;
use fixed_decimal::Decimal;
use icu_decimal::options::DecimalFormatterOptions;
use icu_decimal::DecimalFormatter;
use icu_decimal::DecimalFormatterPreferences;
use icu_locale::DataLocale;
use icu_locale_core::preferences::{define_preferences, prefs_convert};
use icu_plurals::PluralRules;
use icu_plurals::PluralRulesPreferences;
use icu_provider::marker::DataMarkerExt;
use icu_provider::DataError;
use icu_provider::{
    DataIdentifierBorrowed, DataMarkerAttributes, DataPayload, DataProvider, DataRequest,
};
use smallvec::SmallVec;

/// Type alias for the formatting info needed by the formatter constructors.
type FormattingInfo = (
    DataLocale,
    DecimalFormatter,
    PluralRules,
    // Holds the attributes.
    SmallVec<[u8; 32]>, // TODO: Remove this once we have separate markers for different widths.
);

define_preferences!(
    /// The preferences for units formatting.
    [Copy]
    CategorizedUnitsFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: crate::dimension::preferences::NumberingSystem
    }
);
prefs_convert!(
    CategorizedUnitsFormatterPreferences,
    DecimalFormatterPreferences,
    { numbering_system }
);
prefs_convert!(CategorizedUnitsFormatterPreferences, PluralRulesPreferences);

/// A [`CategorizedFormatter`] is used to format specific units.
///
/// This is useful for type inference and for ensuring that the correct units are used.
#[derive(Debug)]
pub struct CategorizedFormatter<C: MeasureUnitCategory> {
    _category: PhantomData<C>,
    display_name:
        DataPayload<crate::dimension::provider::units::display_names::UnitsDisplayNamesV1>,
    decimal_formatter: DecimalFormatter,
    plural_rules: PluralRules,
}

impl<C: MeasureUnitCategory> CategorizedFormatter<C> {
    // TODO: Remove this function once we have separate markers for different widths.
    #[inline]
    fn attribute(width: Width, unit: &str) -> SmallVec<[u8; 32]> {
        let mut buffer: SmallVec<[u8; 32]> = SmallVec::new();
        let length = match width {
            Width::Short => "short-",
            Width::Narrow => "narrow-",
            Width::Long => "long-",
        };
        buffer.extend_from_slice(length.as_bytes());
        buffer.extend_from_slice(unit.as_bytes());
        buffer
    }

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CategorizedUnitsFormatterPreferences,
            categorized_unit: CategorizedMeasureUnit<C>,
            options: super::options::UnitsFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_core: skip,
            try_new_core_with_buffer_provider,
            try_new_core_unstable,
            Self
        ]
    );

    icu_provider::gen_buffer_data_constructors!(
        (
            prefs: CategorizedUnitsFormatterPreferences,
            categorized_unit: CategorizedMeasureUnit<C>,
            options: super::options::UnitsFormatterOptions
        ) -> error: DataError,
        functions: [
            try_new_extended: skip,
            try_new_extended_with_buffer_provider,
            try_new_extended_unstable,
            Self
        ]
    );

    /// Extracts the formatting info.
    ///
    /// This is a helper function for the constructors to avoid writing the same code multiple times.
    #[cfg(feature = "compiled_data")]
    fn extract_formatting_info(
        prefs: CategorizedUnitsFormatterPreferences,
        categorized_unit: CategorizedMeasureUnit<C>,
        options: super::options::UnitsFormatterOptions,
    ) -> Result<FormattingInfo, DataError> {
        let locale = C::DataMarkerCore::make_locale(prefs.locale_preferences);
        let decimal_formatter: DecimalFormatter =
            DecimalFormatter::try_new((&prefs).into(), DecimalFormatterOptions::default())?;

        let plural_rules = PluralRules::try_new_cardinal((&prefs).into())?;

        // TODO: Remove this allocation once we have separate markers for different widths.
        let attribute = Self::attribute(options.width, categorized_unit.cldr_id());

        Ok((locale, decimal_formatter, plural_rules, attribute))
    }

    /// Extracts the formatting info for that needed to create an unstable creator of [`CategorizedFormatter`].
    ///
    /// This is a helper function for the constructors to avoid writing the same code multiple times.
    fn extract_formatting_info_unstable<D>(
        provider: &D,
        prefs: CategorizedUnitsFormatterPreferences,
        categorized_unit: CategorizedMeasureUnit<C>,
        options: super::options::UnitsFormatterOptions,
    ) -> Result<FormattingInfo, DataError>
    where
        D: ?Sized
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        let locale = C::DataMarkerCore::make_locale(prefs.locale_preferences);
        let decimal_formatter: DecimalFormatter = DecimalFormatter::try_new_unstable(
            provider,
            (&prefs).into(),
            DecimalFormatterOptions::default(),
        )?;

        let plural_rules = PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?;

        // TODO: Remove this allocation once we have separate markers for different widths.
        let attribute = Self::attribute(options.width, categorized_unit.cldr_id());

        Ok((locale, decimal_formatter, plural_rules, attribute))
    }

    /// Creates a new [`CategorizedFormatter`] from compiled locale data and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_core(
        prefs: CategorizedUnitsFormatterPreferences,
        categorized_unit: CategorizedMeasureUnit<C>,
        options: super::options::UnitsFormatterOptions,
    ) -> Result<Self, DataError>
    where
        crate::provider::Baked: DataProvider<C::DataMarkerCore>,
    {
        let (locale, decimal_formatter, plural_rules, attribute) =
            Self::extract_formatting_info(prefs, categorized_unit, options)?;
        let unit_attribute = DataMarkerAttributes::try_from_utf8(&attribute[..attribute.len()])
            .map_err(|_| DataError::custom("Failed to create a data marker"))?;

        let display_name = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    unit_attribute,
                    &locale,
                ),
                ..Default::default()
            })?
            .payload
            .cast();

        Ok(Self {
            _category: PhantomData,
            display_name,
            decimal_formatter,
            plural_rules,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_core)]
    pub fn try_new_core_unstable<D>(
        provider: &D,
        prefs: CategorizedUnitsFormatterPreferences,
        categorized_unit: CategorizedMeasureUnit<C>,
        options: super::options::UnitsFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<C::DataMarkerCore>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        let (locale, decimal_formatter, plural_rules, attribute) =
            Self::extract_formatting_info_unstable(provider, prefs, categorized_unit, options)?;

        let unit_attribute = DataMarkerAttributes::try_from_utf8(&attribute[..attribute.len()])
            .map_err(|_| DataError::custom("Failed to create a data marker"))?;

        let display_name = DataProvider::<C::DataMarkerCore>::load(
            provider,
            DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    unit_attribute,
                    &locale,
                ),
                ..Default::default()
            },
        )?
        .payload
        .cast();

        Ok(Self {
            _category: PhantomData,
            display_name,
            decimal_formatter,
            plural_rules,
        })
    }

    /// Creates a new [`CategorizedFormatter`] from compiled locale data with core and extended markers and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_extended(
        prefs: CategorizedUnitsFormatterPreferences,
        categorized_unit: CategorizedMeasureUnit<C>,
        options: super::options::UnitsFormatterOptions,
    ) -> Result<Self, DataError>
    where
        crate::provider::Baked: DataProvider<C::DataMarkerCore>,
        crate::provider::Baked: DataProvider<C::DataMarkerExtended>,
    {
        let (locale, decimal_formatter, plural_rules, attribute) =
            Self::extract_formatting_info(prefs, categorized_unit, options)?;
        let unit_attribute = DataMarkerAttributes::try_from_utf8(&attribute[..attribute.len()])
            .map_err(|_| DataError::custom("Failed to create a data marker"))?;

        let req = DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes_and_locale(unit_attribute, &locale),
            ..Default::default()
        };

        let display_name = <crate::provider::Baked as DataProvider<C::DataMarkerCore>>::load(
            &crate::provider::Baked,
            req,
        )
        .map(|r| r.payload.cast())
        .or_else(|_| {
            <crate::provider::Baked as DataProvider<C::DataMarkerExtended>>::load(
                &crate::provider::Baked,
                req,
            )
            .map(|r| r.payload.cast())
        })?;

        Ok(Self {
            _category: PhantomData,
            display_name,
            decimal_formatter,
            plural_rules,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_extended)]
    pub fn try_new_extended_unstable<D>(
        provider: &D,
        prefs: CategorizedUnitsFormatterPreferences,
        categorized_unit: CategorizedMeasureUnit<C>,
        options: super::options::UnitsFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<C::DataMarkerCore>
            + DataProvider<C::DataMarkerExtended>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        let (locale, decimal_formatter, plural_rules, attribute) =
            Self::extract_formatting_info_unstable(provider, prefs, categorized_unit, options)?;

        let unit_attribute = DataMarkerAttributes::try_from_utf8(&attribute[..attribute.len()])
            .map_err(|_| DataError::custom("Failed to create a data marker"))?;

        let req = DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes_and_locale(unit_attribute, &locale),
            ..Default::default()
        };

        let display_name = DataProvider::<C::DataMarkerCore>::load(provider, req)
            .map(|r| r.payload.cast())
            .or_else(|_| {
                DataProvider::<C::DataMarkerExtended>::load(provider, req).map(|r| r.payload.cast())
            })?;

        Ok(Self {
            _category: PhantomData,
            display_name,
            decimal_formatter,
            plural_rules,
        })
    }

    /// Creates a new [`CategorizedFormatter`] from compiled locale data with core, extended and outlier markers and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_outlier(
        prefs: CategorizedUnitsFormatterPreferences,
        categorized_unit: CategorizedMeasureUnit<C>,
        options: super::options::UnitsFormatterOptions,
    ) -> Result<Self, DataError>
    where
        crate::provider::Baked: DataProvider<C::DataMarkerCore>,
        crate::provider::Baked: DataProvider<C::DataMarkerExtended>,
        crate::provider::Baked: DataProvider<C::DataMarkerOutlier>,
    {
        let (locale, decimal_formatter, plural_rules, attribute) =
            Self::extract_formatting_info(prefs, categorized_unit, options)?;
        let unit_attribute = DataMarkerAttributes::try_from_utf8(&attribute[..attribute.len()])
            .map_err(|_| DataError::custom("Failed to create a data marker"))?;

        let req = DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes_and_locale(unit_attribute, &locale),
            ..Default::default()
        };

        let display_name = <crate::provider::Baked as DataProvider<C::DataMarkerCore>>::load(
            &crate::provider::Baked,
            req,
        )
        .map(|r| r.payload.cast())
        .or_else(|_| {
            <crate::provider::Baked as DataProvider<C::DataMarkerExtended>>::load(
                &crate::provider::Baked,
                req,
            )
            .map(|r| r.payload.cast())
        })
        .or_else(|_| {
            <crate::provider::Baked as DataProvider<C::DataMarkerOutlier>>::load(
                &crate::provider::Baked,
                req,
            )
            .map(|r| r.payload.cast())
        })?;

        Ok(Self {
            _category: PhantomData,
            display_name,
            decimal_formatter,
            plural_rules,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_outlier)]
    pub fn try_new_outlier_unstable<D>(
        provider: &D,
        prefs: CategorizedUnitsFormatterPreferences,
        categorized_unit: CategorizedMeasureUnit<C>,
        options: super::options::UnitsFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<C::DataMarkerCore>
            + DataProvider<C::DataMarkerExtended>
            + DataProvider<C::DataMarkerOutlier>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        let (locale, decimal_formatter, plural_rules, attribute) =
            Self::extract_formatting_info_unstable(provider, prefs, categorized_unit, options)?;

        let unit_attribute = DataMarkerAttributes::try_from_utf8(&attribute[..attribute.len()])
            .map_err(|_| DataError::custom("Failed to create a data marker"))?;

        let req = DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes_and_locale(unit_attribute, &locale),
            ..Default::default()
        };

        let display_name = DataProvider::<C::DataMarkerCore>::load(provider, req)
            .map(|r| r.payload.cast())
            .or_else(|_| {
                DataProvider::<C::DataMarkerExtended>::load(provider, req).map(|r| r.payload.cast())
            })
            .or_else(|_| {
                DataProvider::<C::DataMarkerOutlier>::load(provider, req).map(|r| r.payload.cast())
            })?;

        Ok(Self {
            _category: PhantomData,
            display_name,
            decimal_formatter,
            plural_rules,
        })
    }

    /// Formats a [`Decimal`] value for the given unit.
    pub fn format_fixed_decimal<'l>(&'l self, value: &'l Decimal) -> FormattedUnit<'l> {
        FormattedUnit {
            value,
            display_name: self.display_name.get(),
            decimal_formatter: &self.decimal_formatter,
            plural_rules: &self.plural_rules,
        }
    }
}

#[cfg(feature = "compiled_data")]
#[cfg(test)]
mod tests {
    use core::str::FromStr;
    use fixed_decimal::Decimal;
    use icu_locale_core::locale;
    use writeable::assert_writeable_eq;

    use crate::dimension::units::categorized_formatter::CategorizedFormatter;
    use crate::dimension::units::options::{UnitsFormatterOptions, Width};
    use crate::measure::category::{Area, Duration};

    #[test]
    fn test_area_categorized_core_formatter() {
        let test_cases = vec![
            (
                locale!("en-US"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions::default(),
                "1,000 m²",
            ),
            (
                locale!("en-US"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "1,000 square meters",
            ),
            (
                locale!("fr-FR"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "1\u{202f}000\u{a0}mètres carrés",
            ),
            (
                locale!("ar-EG"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "١٬٠٠٠ متر مربع",
            ),
        ];

        for (locale, categorized_unit, value_str, options, expected) in test_cases {
            let formatter = CategorizedFormatter::<Area>::try_new_core(
                locale.clone().into(),
                categorized_unit,
                options,
            );

            if locale.to_string() == "en-US" {
                assert!(
                    formatter.is_err(),
                    "Expected failure for the en-US locale because the unit is not available in core"
                );
                continue;
            }
            let formatter = formatter.unwrap();
            let signed_decimal = Decimal::from_str(value_str).unwrap();
            let formatted = formatter.format_fixed_decimal(&signed_decimal);

            assert_writeable_eq!(formatted, expected);
        }
    }

    #[test]
    fn test_area_categorized_extended_formatter() {
        let test_cases = vec![
            (
                locale!("en-US"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions::default(),
                "1,000 m²",
            ),
            (
                locale!("en-US"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "1,000 square meters",
            ),
            (
                locale!("fr-FR"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "1\u{202f}000\u{a0}mètres carrés",
            ),
            (
                locale!("ar-EG"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "١٬٠٠٠ متر مربع",
            ),
        ];

        for (locale, categorized_unit, value_str, options, expected) in test_cases {
            let formatter = CategorizedFormatter::<Area>::try_new_extended(
                locale.clone().into(),
                categorized_unit,
                options,
            );
            let formatter = formatter.unwrap();
            let signed_decimal = Decimal::from_str(value_str).unwrap();
            let formatted = formatter.format_fixed_decimal(&signed_decimal);

            assert_writeable_eq!(formatted, expected);
        }
    }

    #[test]
    fn test_area_categorized_outlier_formatter() {
        let test_cases = vec![
            // TODO: Uncomment this test case once we have danum units in the Area category.
            //    (
            //     locale!("en-US"),
            //     Area::danum(),
            //     "1000",
            //     UnitsFormatterOptions::default(),
            //     "TBD",
            //    ),
            (
                locale!("en-US"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions::default(),
                "1,000 m²",
            ),
            (
                locale!("en-US"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "1,000 square meters",
            ),
            (
                locale!("fr-FR"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "1\u{202f}000\u{a0}mètres carrés",
            ),
            (
                locale!("ar-EG"),
                Area::square_meter(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "١٬٠٠٠ متر مربع",
            ),
        ];

        for (locale, categorized_unit, value_str, options, expected) in test_cases {
            let formatter = CategorizedFormatter::<Area>::try_new_outlier(
                locale.clone().into(),
                categorized_unit,
                options,
            );
            let formatter = formatter.unwrap();
            let signed_decimal = Decimal::from_str(value_str).unwrap();
            let formatted = formatter.format_fixed_decimal(&signed_decimal);

            assert_writeable_eq!(formatted, expected);
        }
    }

    #[test]
    fn test_duration_categorized_core_formatter() {
        let test_cases = vec![
            (
                locale!("en-US"),
                Duration::second(),
                "1000",
                UnitsFormatterOptions::default(),
                "1,000 sec",
            ),
            (
                locale!("en-US"),
                Duration::second(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "1,000 seconds",
            ),
            (
                locale!("fr-FR"),
                Duration::second(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "1\u{202f}000\u{a0}secondes",
            ),
            (
                locale!("ar-EG"),
                Duration::second(),
                "1000",
                UnitsFormatterOptions {
                    width: Width::Long,
                    ..Default::default()
                },
                "١٬٠٠٠ ثانية",
            ),
        ];

        for (locale, categorized_unit, value_str, options, expected) in test_cases {
            let formatter = CategorizedFormatter::<Duration>::try_new_core(
                locale.into(),
                categorized_unit,
                options,
            )
            .unwrap();
            let signed_decimal = Decimal::from_str(value_str).unwrap();
            let formatted = formatter.format_fixed_decimal(&signed_decimal);
            assert_writeable_eq!(formatted, expected);
        }
    }
}
