// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::Decimal;
use icu_decimal::options::DecimalFormatterOptions;
use icu_decimal::{DecimalFormatter, DecimalFormatterPreferences};
use icu_locale_core::preferences::{define_preferences, prefs_convert};
use icu_plurals::{PluralRules, PluralRulesPreferences};
use icu_provider::DataPayload;

use super::format::FormattedUnit;
use super::options::{UnitsFormatterOptions, Width};
use crate::dimension::provider::units::display_names::UnitsDisplayNamesV1;
use icu_provider::prelude::*;
use smallvec::SmallVec;

extern crate alloc;

define_preferences!(
    /// The preferences for units formatting.
    [Copy]
    UnitsFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: crate::dimension::preferences::NumberingSystem
    }
);
prefs_convert!(UnitsFormatterPreferences, DecimalFormatterPreferences, {
    numbering_system
});
prefs_convert!(UnitsFormatterPreferences, PluralRulesPreferences);

// TODO(#6900): Remove the units formatter after migrating all the code to the categorized formatter.
/// A formatter for measurement unit values.
///
/// [`UnitsFormatter`] supports:
///   1. Rendering in the locale's units system.
///   2. Locale-sensitive grouping separator positions.
///
/// Read more about the options in the [`super::options`] module.
#[derive(Debug)]
pub struct UnitsFormatter {
    /// Options bag for the units formatter to determine the behavior of the formatter.
    /// for example: width of the units.
    _options: UnitsFormatterOptions,

    // /// Essential data for the units formatter.
    // essential: DataPayload<UnitsEssentialsV1>,
    /// Display name for the units.
    display_name: DataPayload<UnitsDisplayNamesV1>,

    /// A [`DecimalFormatter`] to format the unit value.
    decimal_formatter: DecimalFormatter,

    /// A [`PluralRules`] to determine the plural category of the unit.
    plural_rules: PluralRules,
}

impl UnitsFormatter {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: UnitsFormatterPreferences, unit: &str, options: UnitsFormatterOptions) -> error: DataError,
        functions: [
            try_new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

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

    /// Creates a new [`UnitsFormatter`] from compiled locale data and an options bag.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new(
        prefs: UnitsFormatterPreferences,
        unit: &str,
        options: UnitsFormatterOptions,
    ) -> Result<Self, DataError> {
        let locale = UnitsDisplayNamesV1::make_locale(prefs.locale_preferences);
        let decimal_formatter =
            DecimalFormatter::try_new((&prefs).into(), DecimalFormatterOptions::default())?;

        let plural_rules = PluralRules::try_new_cardinal((&prefs).into())?;

        // TODO: Remove this allocation once we have separate markers for different widths.
        let attribute = Self::attribute(options.width, unit);
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
            .payload;

        Ok(Self {
            _options: options,
            display_name,
            decimal_formatter,
            plural_rules,
        })
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D>(
        provider: &D,
        prefs: UnitsFormatterPreferences,
        unit: &str,
        options: UnitsFormatterOptions,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<UnitsDisplayNamesV1>
            + DataProvider<icu_decimal::provider::DecimalSymbolsV1>
            + DataProvider<icu_decimal::provider::DecimalDigitsV1>
            + DataProvider<icu_plurals::provider::PluralsCardinalV1>,
    {
        let locale = UnitsDisplayNamesV1::make_locale(prefs.locale_preferences);
        let decimal_formatter = DecimalFormatter::try_new_unstable(
            provider,
            (&prefs).into(),
            DecimalFormatterOptions::default(),
        )?;

        let plural_rules = PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?;

        // TODO: Remove this allocation once we have separate markers for different widths.
        let attribute = Self::attribute(options.width, unit);
        let unit_attribute = DataMarkerAttributes::try_from_utf8(&attribute[..attribute.len()])
            .map_err(|_| DataError::custom("Failed to create a data marker"))?;

        let display_name = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    unit_attribute,
                    &locale,
                ),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            _options: options,
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
