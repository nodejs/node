// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

pub mod si_prefix;
pub mod single_unit;

icu_provider::data_marker!(
    // UnitIdsV1
    UnitIdsV1,
    u16,
    #[cfg(feature = "datagen")]
    expose_baked_consts = true,
);
