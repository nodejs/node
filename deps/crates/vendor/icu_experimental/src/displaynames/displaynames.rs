// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Displaynames component.

use crate::displaynames::options::*;
use crate::displaynames::provider::*;
use alloc::borrow::Cow;
use alloc::string::String;
use icu_locale_core::preferences::define_preferences;
use icu_locale_core::{
    subtags::Language, subtags::Region, subtags::Script, subtags::Variant, LanguageIdentifier,
    Locale,
};
use icu_provider::prelude::*;
use potential_utf::PotentialUtf8;

define_preferences!(
    /// The preferences for display names.
    [Copy]
    DisplayNamesPreferences,
    {}
);

/// Lookup of the locale-specific display names by region code.
///
/// # Examples
///
/// ```
/// use icu::experimental::displaynames::{
///     DisplayNamesOptions, multi::RegionDisplayNames,
/// };
/// use icu::locale::{locale, subtags::region};
///
/// let locale = locale!("en-001").into();
/// let options: DisplayNamesOptions = Default::default();
/// let display_name = RegionDisplayNames::try_new(locale, options)
///     .expect("Data should load successfully");
///
/// assert_eq!(display_name.of(region!("AE")), Some("United Arab Emirates"));
/// ```
#[derive(Default, Debug)]
pub struct RegionDisplayNames {
    options: DisplayNamesOptions,
    region_data: DataPayload<RegionDisplayNamesV1>,
}

impl RegionDisplayNames {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, options: DisplayNamesOptions) -> error: DataError,
        /// Creates a new [`RegionDisplayNames`] from locale data and an options bag using compiled data.
        functions: [
            try_new,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D: DataProvider<RegionDisplayNamesV1> + ?Sized>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        options: DisplayNamesOptions,
    ) -> Result<Self, DataError> {
        let locale = RegionDisplayNamesV1::make_locale(prefs.locale_preferences);
        let region_data = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            options,
            region_data,
        })
    }

    /// Returns the display name of a region.
    pub fn of(&self, region: Region) -> Option<&str> {
        let data = self.region_data.get();
        match self.options.style {
            Some(Style::Short) => data.short_names.get(&region.to_tinystr().to_unvalidated()),
            _ => None,
        }
        .or_else(|| data.names.get(&region.to_tinystr().to_unvalidated()))
        // TODO: Respect options.fallback
    }
}

/// Lookup of the locale-specific display names by script code.
///
/// # Examples
///
/// ```
/// use icu::experimental::displaynames::{
///     DisplayNamesOptions, multi::ScriptDisplayNames,
/// };
/// use icu::locale::{locale, subtags::script};
///
/// let locale = locale!("en-001").into();
/// let options: DisplayNamesOptions = Default::default();
/// let display_name = ScriptDisplayNames::try_new(locale, options)
///     .expect("Data should load successfully");
///
/// assert_eq!(display_name.of(script!("Maya")), Some("Mayan hieroglyphs"));
/// ```
#[derive(Default, Debug)]
pub struct ScriptDisplayNames {
    options: DisplayNamesOptions,
    script_data: DataPayload<ScriptDisplayNamesV1>,
}

impl ScriptDisplayNames {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, options: DisplayNamesOptions) -> error: DataError,
        /// Creates a new [`ScriptDisplayNames`] from locale data and an options bag using compiled data.
        functions: [
            try_new,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D: DataProvider<ScriptDisplayNamesV1> + ?Sized>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        options: DisplayNamesOptions,
    ) -> Result<Self, DataError> {
        let locale = ScriptDisplayNamesV1::make_locale(prefs.locale_preferences);
        let script_data = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            options,
            script_data,
        })
    }

    /// Returns the display name of a script.
    pub fn of(&self, script: Script) -> Option<&str> {
        let data = self.script_data.get();
        match self.options.style {
            Some(Style::Short) => data.short_names.get(&script.to_tinystr().to_unvalidated()),
            _ => None,
        }
        .or_else(|| data.names.get(&script.to_tinystr().to_unvalidated()))
        // TODO: Respect options.fallback
    }
}

/// Lookup of the locale-specific display names by variant.
///
/// # Examples
///
/// ```
/// use icu::experimental::displaynames::{
///     DisplayNamesOptions, multi::VariantDisplayNames,
/// };
/// use icu::locale::{locale, subtags::variant};
///
/// let locale = locale!("en-001").into();
/// let options: DisplayNamesOptions = Default::default();
/// let display_name = VariantDisplayNames::try_new(locale, options)
///     .expect("Data should load successfully");
///
/// assert_eq!(display_name.of(variant!("POSIX")), Some("Computer"));
/// ```
#[derive(Default, Debug)]
pub struct VariantDisplayNames {
    #[allow(dead_code)] //TODO: Add DisplayNamesOptions support for Variants.
    options: DisplayNamesOptions,
    variant_data: DataPayload<VariantDisplayNamesV1>,
}

impl VariantDisplayNames {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, options: DisplayNamesOptions) -> error: DataError,
        /// Creates a new [`VariantDisplayNames`] from locale data and an options bag using compiled data.
        functions: [
            try_new,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D: DataProvider<VariantDisplayNamesV1> + ?Sized>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        options: DisplayNamesOptions,
    ) -> Result<Self, DataError> {
        let locale = VariantDisplayNamesV1::make_locale(prefs.locale_preferences);
        let variant_data = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            options,
            variant_data,
        })
    }

    /// Returns the display name of a variant.
    pub fn of(&self, variant: Variant) -> Option<&str> {
        let data = self.variant_data.get();
        data.names.get(&variant.to_tinystr().to_unvalidated())
        // TODO: Respect options.fallback
    }
}

/// Lookup of the locale-specific display names by language code.
///
/// # Examples
///
/// ```
/// use icu::experimental::displaynames::{
///     DisplayNamesOptions, multi::LanguageDisplayNames,
/// };
/// use icu::locale::{locale, subtags::language};
///
/// let locale = locale!("en-001").into();
/// let options: DisplayNamesOptions = Default::default();
/// let display_name = LanguageDisplayNames::try_new(locale, options)
///     .expect("Data should load successfully");
///
/// assert_eq!(display_name.of(language!("de")), Some("German"));
/// ```
#[derive(Default, Debug)]
pub struct LanguageDisplayNames {
    options: DisplayNamesOptions,
    language_data: DataPayload<LanguageDisplayNamesV1>,
}

impl LanguageDisplayNames {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, options: DisplayNamesOptions) -> error: DataError,
        /// Creates a new [`LanguageDisplayNames`] from locale data and an options bag using compiled data.
        functions: [
            try_new,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D: DataProvider<LanguageDisplayNamesV1> + ?Sized>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        options: DisplayNamesOptions,
    ) -> Result<Self, DataError> {
        let locale = LanguageDisplayNamesV1::make_locale(prefs.locale_preferences);
        let language_data = provider
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                ..Default::default()
            })?
            .payload;

        Ok(Self {
            options,
            language_data,
        })
    }

    /// Returns the display name of a language.
    pub fn of(&self, language: Language) -> Option<&str> {
        let data = self.language_data.get();
        match self.options.style {
            Some(Style::Short) => data
                .short_names
                .get(&language.to_tinystr().to_unvalidated()),
            Some(Style::Long) => data.long_names.get(&language.to_tinystr().to_unvalidated()),
            Some(Style::Menu) => data.menu_names.get(&language.to_tinystr().to_unvalidated()),
            _ => None,
        }
        .or_else(|| data.names.get(&language.to_tinystr().to_unvalidated()))
        // TODO: Respect options.fallback
    }
}

/// Format a locale as a display string.
///
/// # Examples
///
/// ```
/// use icu::experimental::displaynames::{
///     DisplayNamesOptions, multi::LocaleDisplayNamesFormatter,
/// };
/// use icu::locale::locale;
///
/// let locale = locale!("en-001").into();
/// let options: DisplayNamesOptions = Default::default();
/// let display_name = LocaleDisplayNamesFormatter::try_new(locale, options)
///     .expect("Data should load successfully");
///
/// assert_eq!(display_name.of(&locale!("en-GB")), "British English");
/// assert_eq!(display_name.of(&locale!("en")), "English");
/// assert_eq!(display_name.of(&locale!("en-MX")), "English (Mexico)");
/// assert_eq!(display_name.of(&locale!("xx-YY")), "xx (YY)");
/// assert_eq!(display_name.of(&locale!("xx")), "xx");
/// ```
#[derive(Debug)]
pub struct LocaleDisplayNamesFormatter {
    options: DisplayNamesOptions,
    // patterns: DataPayload<LocaleDisplayNamesPatternsV1>,
    locale_data: DataPayload<LocaleDisplayNamesV1>,

    language_data: DataPayload<LanguageDisplayNamesV1>,
    script_data: DataPayload<ScriptDisplayNamesV1>,
    region_data: DataPayload<RegionDisplayNamesV1>,
    variant_data: DataPayload<VariantDisplayNamesV1>,
    // key_data: DataPayload<KeyDisplayNamesV1>,
    // measurement_data: DataPayload<MeasurementSystemsDisplayNamesV1>,
    // subdivisions_data: DataPayload<SubdivisionsDisplayNamesV1>,
    // transforms_data: DataPayload<TransformsDisplayNamesV1>,
}

impl LocaleDisplayNamesFormatter {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, options: DisplayNamesOptions) -> error: DataError,
        /// Creates a new [`LocaleDisplayNamesFormatter`] from locale data and an options bag using compiled data.
        functions: [
            try_new,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        options: DisplayNamesOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<LocaleDisplayNamesV1>
            + DataProvider<LanguageDisplayNamesV1>
            + DataProvider<ScriptDisplayNamesV1>
            + DataProvider<RegionDisplayNamesV1>
            + DataProvider<VariantDisplayNamesV1>
            + ?Sized,
    {
        let locale = LocaleDisplayNamesV1::make_locale(prefs.locale_preferences);
        let req = DataRequest {
            id: DataIdentifierBorrowed::for_locale(&locale),
            ..Default::default()
        };

        Ok(Self {
            options,
            language_data: provider.load(req)?.payload,
            locale_data: provider.load(req)?.payload,
            script_data: provider.load(req)?.payload,
            region_data: provider.load(req)?.payload,
            variant_data: provider.load(req)?.payload,
        })
    }

    /// Returns the display name of a locale.
    /// This implementation is based on the algorithm described in
    /// <https://www.unicode.org/reports/tr35/tr35-general.html#locale_display_name_algorithm>
    // TODO: Make this return a writeable instead of using alloc
    pub fn of<'a, 'b: 'a, 'c: 'a>(&'b self, locale: &'c Locale) -> Cow<'a, str> {
        // Step - 1: Construct a locale display name string (LDN) for the longest matching subtag.
        let mut ldn = None;

        // These qualifying strings (QS) will later be set to None if they are part of the LDN.
        // Using references as we might borrow from these later
        let mut script_qs = locale.id.script.as_ref();
        let mut region_qs = locale.id.region.as_ref();
        let variants_qs = Cow::Borrowed(&*locale.id.variants);

        // TODO: handle the other possible longest subtag cases

        // If not in Dialect mode, we always assemble the display names from language, script, region
        if self.options.language_display == LanguageDisplay::Dialect {
            if let Some(script) = locale.id.script {
                let data = self.locale_data.get();
                let id = LanguageIdentifier::from((locale.id.language, Some(script), None));
                let cmp = |uvstr: &PotentialUtf8| id.strict_cmp(uvstr).reverse();
                if let Some(x) = match self.options.style {
                    Some(Style::Short) => data.short_names.get_by(cmp),
                    Some(Style::Long) => data.long_names.get_by(cmp),
                    Some(Style::Menu) => data.menu_names.get_by(cmp),
                    _ => None,
                }
                .or_else(|| data.names.get_by(cmp))
                {
                    ldn = Some(x);
                    script_qs = None;
                }
            }

            if ldn.is_none() {
                if let Some(region) = locale.id.region {
                    let data = self.locale_data.get();
                    let id = LanguageIdentifier::from((locale.id.language, None, Some(region)));
                    let cmp = |uvstr: &PotentialUtf8| id.strict_cmp(uvstr).reverse();
                    if let Some(x) = match self.options.style {
                        Some(Style::Short) => data.short_names.get_by(cmp),
                        Some(Style::Long) => data.long_names.get_by(cmp),
                        Some(Style::Menu) => data.menu_names.get_by(cmp),
                        _ => None,
                    }
                    .or_else(|| data.names.get_by(cmp))
                    {
                        ldn = Some(x);
                        region_qs = None;
                    }
                }
            }
        }

        let ldn = ldn
            .or_else(|| {
                let data = self.language_data.get();
                let key = locale.id.language.to_tinystr().to_unvalidated();
                match self.options.style {
                    Some(Style::Short) => data.short_names.get(&key),
                    Some(Style::Long) => data.long_names.get(&key),
                    Some(Style::Menu) => data.menu_names.get(&key),
                    _ => None,
                }
                .or_else(|| data.names.get(&key))
            })
            .unwrap_or(locale.id.language.as_str());

        if script_qs.is_none() && region_qs.is_none() && variants_qs.is_empty() {
            // The LDN fully represents the locale
            return Cow::Borrowed(ldn);
        }

        // Step - 2: Construct the list of qualifying substrings (LQS).

        let script_qs = script_qs.map(|script| {
            let data = self.script_data.get();
            let key = script.to_tinystr().to_unvalidated();
            match self.options.style {
                Some(Style::Short) => data.short_names.get(&key),
                _ => None,
            }
            .or_else(|| data.names.get(&key))
            .unwrap_or(script.as_str())
        });

        let region_qs = region_qs.map(|region| {
            let data = self.region_data.get();
            let key = region.to_tinystr().to_unvalidated();
            match self.options.style {
                Some(Style::Short) => data.short_names.get(&key),
                _ => None,
            }
            .or_else(|| data.names.get(&key))
            .unwrap_or(region.as_str())
        });

        let variants_qs = variants_qs.iter().map(|variant_key| {
            self.variant_data
                .get()
                .names
                .get(&variant_key.to_tinystr().to_unvalidated())
                .unwrap_or(variant_key.as_str())
        });

        let lqs = script_qs.into_iter().chain(region_qs).chain(variants_qs);

        // Step - 3: Write LDN and LQS to output
        // TODO: Move to an `impl Writeable`

        // TODO: load from data
        let (before, middle, after) = (" (", ", ", ")");

        let mut output = String::with_capacity(
            ldn.len() + before.len() + lqs.clone().map(|x| x.len() + middle.len()).sum::<usize>()
                - middle.len()
                + after.len(),
        );
        output.push_str(ldn);
        output.push_str(before);

        let mut first = true;
        for lqs in lqs {
            if !first {
                output.push_str(middle);
            } else {
                first = false;
            }
            output.push_str(lqs);
        }
        output.push_str(after);
        Cow::Owned(output)
    }
}

#[test]
fn test_language_display() {
    use icu_locale_core::locale;

    let dialect = LocaleDisplayNamesFormatter::try_new(
        locale!("en").into(),
        DisplayNamesOptions {
            language_display: LanguageDisplay::Dialect,
            ..Default::default()
        },
    )
    .unwrap();
    let standard = LocaleDisplayNamesFormatter::try_new(
        locale!("en").into(),
        DisplayNamesOptions {
            language_display: LanguageDisplay::Standard,
            ..Default::default()
        },
    )
    .unwrap();

    assert_eq!(dialect.of(&locale!("en-GB")), "British English");
    assert_eq!(standard.of(&locale!("en-GB")), "English (United Kingdom)");

    assert_eq!(dialect.of(&locale!("zh-Hant")), "Traditional Chinese");
    assert_eq!(standard.of(&locale!("zh-Hant")), "Chinese (Traditional)");
}
