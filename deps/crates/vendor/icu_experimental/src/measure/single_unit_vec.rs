// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::provider::single_unit::SingleUnit;

use alloc::{vec, vec::Vec};

// The SingleUnitVec enum is used to represent a collection of SingleUnit instances.
// It can represent zero, one, two, or multiple units, depending on the variant.
// The iter method provides an iterator over the contained SingleUnit instances.
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) enum SingleUnitVec {
    Empty,
    One(SingleUnit),
    Two([SingleUnit; 2]),
    Multi(Vec<SingleUnit>),
}

impl SingleUnitVec {
    /// Returns a slice of the [`SingleUnit`] instances contained
    /// within the [`SingleUnitVec`].
    pub(crate) fn as_slice(&self) -> &[SingleUnit] {
        match self {
            SingleUnitVec::Empty => &[],
            SingleUnitVec::One(unit) => core::slice::from_ref(unit),
            SingleUnitVec::Two(units) => &units[..],
            SingleUnitVec::Multi(units) => &units[..],
        }
    }

    pub(crate) fn push(&mut self, input_unit: SingleUnit) {
        match self {
            SingleUnitVec::Empty => *self = SingleUnitVec::One(input_unit),
            SingleUnitVec::One(current_unit) => {
                *self = SingleUnitVec::Two([*current_unit, input_unit])
            }
            SingleUnitVec::Two(current_units) => {
                *self = SingleUnitVec::Multi(vec![current_units[0], current_units[1], input_unit]);
            }
            SingleUnitVec::Multi(current_units) => current_units.push(input_unit),
        }
    }
}
