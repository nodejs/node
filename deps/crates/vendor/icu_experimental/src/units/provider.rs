// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_provider::prelude::*;
use num_bigint::BigInt;
use zerovec::{maps::ZeroVecLike, ule::AsULE, VarZeroVec, ZeroVec};

use crate::measure::provider::single_unit::{SingleUnit, UnitID};
#[cfg(feature = "compiled_data")]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub use crate::provider::Baked;

use super::ratio::IcuRatio;

icu_provider::data_marker!(UnitsInfoV1, UnitsInfo<'static>, is_singleton = true);

/// This type encapsulates all the constant data required for unit conversions.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, PartialEq, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::units::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct UnitsInfo<'data> {
    /// Contains conversion information sorted by `unit_id`, including conversion rates and base units.
    /// For instance, the conversion for `foot` is represented as `1 foot = 0.3048 meter`.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub conversion_info: VarZeroVec<'data, ConversionInfoULE>,
}

impl UnitsInfo<'_> {
    /// Retrieves the conversion details associated with a specific `unit_id`.
    ///
    /// # Parameters
    ///
    /// * `unit_id` - A unique identifier representing the unit to be located.
    ///
    /// # Returns
    ///
    /// * `Some(&ConversionInfoULE)` - A reference to the conversion information if the `unit_id` is found.
    /// * `None` - If the `unit_id` is not found.
    pub fn conversion_info_by_unit_id(&self, unit_id: UnitID) -> Option<&ConversionInfoULE> {
        self.conversion_info
            .zvl_binary_search_by(|convert_unit| {
                convert_unit.unit_id.as_unsigned_int().cmp(&unit_id)
            })
            .ok()
            .and_then(|index| self.conversion_info.get(index))
    }
}

icu_provider::data_struct!(UnitsInfo<'_>, #[cfg(feature = "datagen")]);

/// Represents the conversion information for a unit.
/// Which includes the base unit (the unit which the unit is converted to), the conversion factor, and the offset.
#[zerovec::make_varule(ConversionInfoULE)]
#[derive(Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Default)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::units::provider))]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize),
    zerovec::derive(Serialize)
)]
#[cfg_attr(
    feature = "serde",
    derive(serde::Deserialize),
    zerovec::derive(Deserialize)
)]
#[zerovec::derive(Debug)]
pub struct ConversionInfo<'data> {
    /// Represents the unique identifier for the unit that is being converted.
    /// For example, when converting from `square-meter`, `unit_id` corresponds to the identifier of `meter`.
    pub unit_id: UnitID,

    /// Contains the base unit (after parsing) which what the unit is converted to.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub basic_units: ZeroVec<'data, SingleUnit>,

    /// Represents the numerator of the conversion factor.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub factor_num: ZeroVec<'data, u8>,

    /// Represents the denominator of the conversion factor.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub factor_den: ZeroVec<'data, u8>,

    /// Represents the sign of the conversion factor.
    pub factor_sign: Sign,

    // TODO(#4311).
    /// Represents the numerator of the offset.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub offset_num: ZeroVec<'data, u8>,

    // TODO(#4311).
    /// Represents the denominator of the offset.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub offset_den: ZeroVec<'data, u8>,

    /// Represents the sign of the offset.
    pub offset_sign: Sign,

    /// Represents the exactness of the conversion factor.
    pub exactness: Exactness,
}

/// This enum is used to represent the sign of a constant value.
#[zerovec::make_ule(SignULE)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::units::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum Sign {
    #[default]
    Positive = 0,
    Negative = 1,
}

/// This enum is used to represent the exactness of a factor
#[zerovec::make_ule(ExactnessULE)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::units::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum Exactness {
    #[default]
    Exact = 0,
    Approximate = 1,
}

impl ConversionInfoULE {
    /// Extracts the conversion factor as [`super::ratio::IcuRatio`].
    pub(crate) fn factor_as_ratio(&self) -> IcuRatio {
        let sign: num_bigint::Sign = Sign::from_unaligned(self.factor_sign).into();
        IcuRatio::from_big_ints(
            BigInt::from_bytes_le(sign, self.factor_num().as_ule_slice()),
            BigInt::from_bytes_le(num_bigint::Sign::Plus, self.factor_den().as_ule_slice()),
        )
    }

    /// Extracts the offset as [`super::ratio::IcuRatio`].
    pub(crate) fn offset_as_ratio(&self) -> IcuRatio {
        let sign: num_bigint::Sign = Sign::from_unaligned(self.offset_sign).into();
        IcuRatio::from_big_ints(
            BigInt::from_bytes_le(sign, self.offset_num().as_ule_slice()),
            BigInt::from_bytes_le(num_bigint::Sign::Plus, self.offset_den().as_ule_slice()),
        )
    }
}
