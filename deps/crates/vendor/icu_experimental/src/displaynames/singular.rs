// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::DisplayNamesPreferences;
use crate::displaynames::provider::*;
use icu_locale::subtags::Script;
use icu_locale_core::subtags::Region;
use icu_provider::prelude::*;
use zerovec::VarZeroCow;

fn try_new_unstable<M, D>(
    provider: &D,
    prefs: DisplayNamesPreferences,
    attr: &str,
) -> Result<DataPayload<M>, DataError>
where
    M: DataMarker<DataStruct = VarZeroCow<'static, str>>,
    D: DataProvider<M> + ?Sized,
{
    let locale = M::make_locale(prefs.locale_preferences);
    let payload = provider
        .load(DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                DataMarkerAttributes::try_from_str(attr)
                    .map_err(|_| DataError::custom("Invalid region"))?,
                &locale,
            ),
            ..Default::default()
        })?
        .payload;
    Ok(payload)
}

fn try_new_short_unstable<MShort, MLong, D>(
    provider: &D,
    prefs: DisplayNamesPreferences,
    attr: &str,
) -> Result<DataPayload<MLong>, DataError>
where
    MShort: DataMarker<DataStruct = VarZeroCow<'static, str>>,
    MLong: DataMarker<DataStruct = VarZeroCow<'static, str>>,
    D: DataProvider<MShort> + DataProvider<MLong> + ?Sized,
{
    let locale = MShort::make_locale(prefs.locale_preferences);
    let id = DataIdentifierBorrowed::for_marker_attributes_and_locale(
        DataMarkerAttributes::try_from_str(attr)
            .map_err(|_| DataError::custom("Invalid region"))?,
        &locale,
    );
    let mut metadata = DataRequestMetadata::default();
    metadata.silent = true;
    let result: Result<DataResponse<MShort>, DataError> =
        provider.load(DataRequest { id, metadata });

    match result {
        Ok(response) => Ok(response.payload.cast()),
        Err(DataError {
            kind: DataErrorKind::IdentifierNotFound,
            ..
        }) => try_new_unstable(provider, prefs, attr),
        Err(e) => Err(e),
    }
}

/// A localized display name for a single script.
///
/// # Example
///
/// ```
/// use icu::experimental::displaynames::single::ScriptDisplayName;
/// use icu::locale::{locale, subtags::script};
/// use writeable::assert_writeable_eq;
///
/// let display_name = ScriptDisplayName::try_new(locale!("en").into(), script!("Xsux"))
///     .expect("Data should load successfully");
///
/// assert_writeable_eq!(display_name, "Sumero-Akkadian Cuneiform");
/// ```
#[derive(Debug)]
pub struct ScriptDisplayName {
    payload: DataPayload<LocaleNamesScriptLongV1>,
}

impl ScriptDisplayName {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, script: Script) -> result: Result<Self, DataError>,
        /// Loads the long script display name for a given script and locale using compiled data.
        functions: [
            try_new,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D: DataProvider<LocaleNamesScriptLongV1> + ?Sized>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        script: Script,
    ) -> Result<Self, DataError> {
        try_new_unstable::<LocaleNamesScriptLongV1, _>(provider, prefs, script.as_str())
            .map(|payload| Self { payload })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, script: Script) -> result: Result<Self, DataError>,
        /// Loads the short script display name for a given script and locale using compiled data.
        ///
        /// Falls back to the long name if the short name is not available.
        ///
        /// # Example
        ///
        /// ```
        /// use icu::experimental::displaynames::{
        ///     DisplayNamesPreferences, single::ScriptDisplayName,
        /// };
        /// use icu::locale::{locale, subtags::script};
        /// use writeable::assert_writeable_eq;
        ///
        /// let prefs: DisplayNamesPreferences = locale!("en-US").into();
        ///
        /// // "Xsux" has a short display name in en-US
        /// let display_name_short = ScriptDisplayName::try_new_short(prefs, script!("Xsux"))
        ///     .expect("Data should load successfully");
        /// assert_writeable_eq!(display_name_short, "S-A Cuneiform");
        ///
        /// // "Deva" does not have a short display name, so it falls back to the long display name
        /// let display_name_long = ScriptDisplayName::try_new_short(prefs, script!("Deva"))
        ///     .expect("Data should load successfully");
        /// assert_writeable_eq!(display_name_long, "Devanagari");
        /// ```
        functions: [
            try_new_short,
            try_new_short_with_buffer_provider,
            try_new_short_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short)]
    pub fn try_new_short_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        script: Script,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<LocaleNamesScriptShortV1> + DataProvider<LocaleNamesScriptLongV1> + ?Sized,
    {
        try_new_short_unstable::<LocaleNamesScriptShortV1, LocaleNamesScriptLongV1, _>(
            provider,
            prefs,
            script.as_str(),
        )
        .map(|payload| Self { payload })
    }
}

impl writeable::Writeable for ScriptDisplayName {
    #[inline]
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        sink.write_str(self.payload.get())
    }

    #[inline]
    fn writeable_length_hint(&self) -> writeable::LengthHint {
        writeable::LengthHint::exact(self.payload.get().len())
    }

    #[inline]
    fn writeable_borrow(&self) -> Option<&str> {
        Some(self.payload.get())
    }
}

writeable::impl_display_with_writeable!(ScriptDisplayName);

/// A localized display name for a single region.
///
/// # Example
///
/// ```
/// use icu::experimental::displaynames::single::RegionDisplayName;
/// use icu::locale::{locale, subtags::region};
/// use writeable::assert_writeable_eq;
///
/// let display_name = RegionDisplayName::try_new(locale!("en").into(), region!("US"))
///     .expect("Data should load successfully");
///
/// assert_writeable_eq!(display_name, "United States");
/// ```
#[derive(Debug)]
pub struct RegionDisplayName {
    payload: DataPayload<LocaleNamesRegionLongV1>,
}

impl RegionDisplayName {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, region: Region) -> result: Result<Self, DataError>,
        /// Loads the long region display name for a given region and locale using compiled data.
        functions: [
            try_new,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<D: DataProvider<LocaleNamesRegionLongV1> + ?Sized>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        region: Region,
    ) -> Result<Self, DataError> {
        try_new_unstable::<LocaleNamesRegionLongV1, _>(provider, prefs, region.as_str())
            .map(|payload| Self { payload })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, region: Region) -> result: Result<Self, DataError>,
        /// Loads the short region display name for a given region and locale using compiled data.
        ///
        /// Falls back to the long name if the short name is not available.
        ///
        /// # Example
        ///
        /// ```
        /// use icu::experimental::displaynames::{
        ///     DisplayNamesPreferences, single::RegionDisplayName,
        /// };
        /// use icu::locale::{locale, subtags::region};
        /// use writeable::assert_writeable_eq;
        ///
        /// let prefs: DisplayNamesPreferences = locale!("en-US").into();
        ///
        /// // "US" has a short display name in en-US
        /// let display_name_short = RegionDisplayName::try_new_short(prefs, region!("US"))
        ///     .expect("Data should load successfully");
        /// assert_writeable_eq!(display_name_short, "US");
        ///
        /// // "FR" does not have a short display name, so it falls back to the long display name
        /// let display_name_long = RegionDisplayName::try_new_short(prefs, region!("FR"))
        ///     .expect("Data should load successfully");
        /// assert_writeable_eq!(display_name_long, "France");
        /// ```
        functions: [
            try_new_short,
            try_new_short_with_buffer_provider,
            try_new_short_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short)]
    pub fn try_new_short_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        region: Region,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<LocaleNamesRegionShortV1> + DataProvider<LocaleNamesRegionLongV1> + ?Sized,
    {
        try_new_short_unstable::<LocaleNamesRegionShortV1, LocaleNamesRegionLongV1, _>(
            provider,
            prefs,
            region.as_str(),
        )
        .map(|payload| Self { payload })
    }
}

impl writeable::Writeable for RegionDisplayName {
    #[inline]
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        sink.write_str(self.payload.get())
    }

    #[inline]
    fn writeable_length_hint(&self) -> writeable::LengthHint {
        writeable::LengthHint::exact(self.payload.get().len())
    }

    #[inline]
    fn writeable_borrow(&self) -> Option<&str> {
        Some(self.payload.get())
    }
}

writeable::impl_display_with_writeable!(RegionDisplayName);
