// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::si_prefix::SiPrefix;
use alloc::string::String;
use core::fmt::Write;

/// Represents a single unit in a measure unit.
/// For example, the [`MeasureUnit`](crate::measure::measureunit::MeasureUnit) `kilometer-per-square-second` contains two single units:
///    1. `kilometer` with power 1 and prefix 3 with base 10.
///    2. `second` with power -2 and prefix power equal to 0.
#[zerovec::make_ule(SingleUnitULE)]
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Default)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::measure::provider::single_unit))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct SingleUnit {
    /// The power of the unit.
    pub power: i8,

    /// The si base of the unit.
    pub si_prefix: SiPrefix,

    /// The id of the unit.
    pub unit_id: UnitID,
}

impl SingleUnit {
    /// Appends the short representation of the single unit to the given string.
    ///
    /// The format of the short representation is as follows:
    /// 1. If the power is not 1, the power is prefixed with "P" followed by the power value.
    /// 2. If the si prefix power is not 0, the si prefix is represented by its base character ('D' for Decimal, 'B' for Binary) followed by the prefix power value.
    /// 3. The unit ID is prefixed with "I" and appended to the string.
    pub(crate) fn append_short_representation(self, buff: &mut String) {
        if self.power != 1 {
            buff.push('P');
            let _infallible = write!(buff, "{}", self.power);
        }

        if self.si_prefix.power != 0 {
            self.si_prefix.append_short_representation(buff);
        }

        buff.push('I');
        let _infallible = write!(buff, "{}", self.unit_id);
    }
}

/// Represents the unique identifier for a unit.
pub type UnitID = u16;
