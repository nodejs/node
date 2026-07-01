// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::marker::PhantomData;

use icu_provider::{DataMarker, DynamicDataMarker};

use crate::{
    dimension::provider::units::{
        categorized_display_names::{
            UnitsNamesAreaCoreV1, UnitsNamesAreaExtendedV1, UnitsNamesAreaOutlierV1,
            UnitsNamesDurationCoreV1, UnitsNamesDurationExtendedV1, UnitsNamesDurationOutlierV1,
            UnitsNamesLengthCoreV1, UnitsNamesLengthExtendedV1, UnitsNamesLengthOutlierV1,
            UnitsNamesMassCoreV1, UnitsNamesMassExtendedV1, UnitsNamesMassOutlierV1,
            UnitsNamesVolumeCoreV1, UnitsNamesVolumeExtendedV1, UnitsNamesVolumeOutlierV1,
        },
        display_names::UnitsDisplayNames,
    },
    measure::measureunit::MeasureUnit,
};

pub mod area;
pub mod duration;
pub mod length;
pub mod mass;
pub mod volume;

pub trait MeasureUnitCategory {
    type DataMarkerCore: DynamicDataMarker<DataStruct = UnitsDisplayNames<'static>> + DataMarker;
    type DataMarkerExtended: DynamicDataMarker<DataStruct = UnitsDisplayNames<'static>> + DataMarker;
    type DataMarkerOutlier: DynamicDataMarker<DataStruct = UnitsDisplayNames<'static>> + DataMarker;
}

/// A [`MeasureUnit`] that is related to a specific category.
///
/// This is useful for type inference and for ensuring that the correct units are used.
#[derive(Debug)]
pub struct CategorizedMeasureUnit<T: MeasureUnitCategory> {
    _category: PhantomData<T>,
    pub unit: MeasureUnit,
}

impl<T: MeasureUnitCategory> CategorizedMeasureUnit<T> {
    // TODO: remove this once we are using the short units name in the datagen to locate the units.
    /// Returns the CLDR ID of the unit.
    pub fn cldr_id(&self) -> &str {
        match self.unit.id {
            Some(id) => id,
            None => unimplemented!(),
        }
    }
}

/// A [`MeasureUnit`] that is related to the area category.
#[derive(Debug)]
#[non_exhaustive]
pub struct Area;

/// A [`MeasureUnit`] that is related to the duration category.
#[derive(Debug)]
#[non_exhaustive]
pub struct Duration;

/// A [`MeasureUnit`] that is related to the length category.
#[derive(Debug)]
#[non_exhaustive]
pub struct Length;

/// A [`MeasureUnit`] that is related to the mass category.
#[derive(Debug)]
#[non_exhaustive]
pub struct Mass;

/// A [`MeasureUnit`] that is related to the volume category.
#[derive(Debug)]
#[non_exhaustive]
pub struct Volume;

impl MeasureUnitCategory for Area {
    type DataMarkerCore = UnitsNamesAreaCoreV1;
    type DataMarkerExtended = UnitsNamesAreaExtendedV1;
    type DataMarkerOutlier = UnitsNamesAreaOutlierV1;
}
impl MeasureUnitCategory for Duration {
    type DataMarkerCore = UnitsNamesDurationCoreV1;
    type DataMarkerExtended = UnitsNamesDurationExtendedV1;
    type DataMarkerOutlier = UnitsNamesDurationOutlierV1;
}
impl MeasureUnitCategory for Length {
    type DataMarkerCore = UnitsNamesLengthCoreV1;
    type DataMarkerExtended = UnitsNamesLengthExtendedV1;
    type DataMarkerOutlier = UnitsNamesLengthOutlierV1;
}
impl MeasureUnitCategory for Mass {
    type DataMarkerCore = UnitsNamesMassCoreV1;
    type DataMarkerExtended = UnitsNamesMassExtendedV1;
    type DataMarkerOutlier = UnitsNamesMassOutlierV1;
}
impl MeasureUnitCategory for Volume {
    type DataMarkerCore = UnitsNamesVolumeCoreV1;
    type DataMarkerExtended = UnitsNamesVolumeExtendedV1;
    type DataMarkerOutlier = UnitsNamesVolumeOutlierV1;
}
