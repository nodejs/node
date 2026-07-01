// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::string::String;
use core::fmt::Write;

/// Represents the base of an si prefix.
#[zerovec::make_ule(BaseULE)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::measure::provider::si_prefix))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum Base {
    /// The base of the si prefix is 10.
    #[default]
    Decimal = 0,

    /// The base of the si prefix is 2.
    Binary = 1,
}

// TODO: Consider reducing the size of this struct while implementing the ULE.
/// Represents the SI prefix.
#[zerovec::make_ule(SiPrefixULE)]
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Default)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::measure::provider::si_prefix))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct SiPrefix {
    /// The absolute value of the power of the si prefix.
    pub power: i8,

    /// The base of the si prefix.
    pub base: Base,
}

impl SiPrefix {
    /// Appends the short representation of the si prefix to the given string.
    pub(crate) fn append_short_representation(self, buff: &mut String) {
        buff.push(match self.base {
            Base::Decimal => 'D',
            Base::Binary => 'B',
        });
        let _infallible = write!(buff, "{}", self.power);
    }
}
