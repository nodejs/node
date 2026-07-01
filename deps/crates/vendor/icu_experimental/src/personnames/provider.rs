// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use alloc::borrow::Cow;
use core::fmt::{Debug, Formatter};

use icu_provider::prelude::*;
#[cfg(feature = "serde")]
use icu_provider::serde_borrow_de_utils::option_of_cow;
use zerovec::VarZeroVec;

use crate::personnames::api::FormattingFormality;
use crate::personnames::api::FormattingLength;
use crate::personnames::api::FormattingOrder;
use crate::personnames::api::FormattingUsage;

#[cfg(feature = "compiled_data")]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub use crate::provider::Baked;

icu_provider::data_marker!(
    /// `PersonNamesFormatV1`
    PersonNamesFormatV1,
    PersonNamesFormat<'static>,
);

/// This is the equivalent of
/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#personnames-element>
///
/// There are some liberties taken from the DTD definition,
/// as we enforce uniqueness on some tags where the documentation doesn't specify any of these
/// constraints.
///
/// e.g. : initialPattern has no upper bound, DTD allows for the element to be specified any number
/// of times, while in this implementation we are restraining it to the 2 documented types
/// (`initial`, `initialSequence`).
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::personnames::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct PersonNamesFormat<'data> {
    /// `<nameOrderLocales order="surnameFirst">ko vi yue zh</nameOrderLocales>`
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub surname_first_locales: VarZeroVec<'data, str>,

    /// `<nameOrderLocales order="givenFirst">und en</nameOrderLocales>`
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub given_first_locales: VarZeroVec<'data, str>,

    /// `foreignSpaceReplacement` element.
    #[cfg_attr(feature = "serde", serde(borrow, deserialize_with = "option_of_cow"))]
    pub foreign_space_replacement: Option<Cow<'data, str>>,

    /// Equivalent of `initialPattern` tag + `initial`
    /// ```xml
    /// <initialPattern type="initial">{0}.</initialPattern>
    /// ```
    #[cfg_attr(feature = "serde", serde(borrow, deserialize_with = "option_of_cow"))]
    pub initial_pattern: Option<Cow<'data, str>>,

    /// Equivalent of `initialPattern` tag + `initialSequence`
    /// ```xml
    /// <initialPattern type="initialSequence">{0} {1}</initialPattern>
    /// ```
    #[cfg_attr(feature = "serde", serde(borrow, deserialize_with = "option_of_cow"))]
    pub initial_pattern_sequence: Option<Cow<'data, str>>,

    /// Equivalent of `personNames`
    /// ```xml
    /// <personName>...</personName>
    /// ```
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub person_names_patterns: VarZeroVec<'data, PersonNamesFormattingDataVarULE>,
}

icu_provider::data_struct!(PersonNamesFormat<'_>, #[cfg(feature = "datagen")]);

/// Person Name Attributes.
/// {order=givenFirst, length=long, usage=referring, formality=formal}
#[derive(Debug)]
pub enum PersonNamesFormattingAttributes {
    GivenFirst,
    SurnameFirst,
    Sorting,
    // Length
    Short,
    Medium,
    Long,
    // Usage
    Addressing,
    Referring,
    Monogram,
    // Formality
    Formal,
    Informal,
}

impl PersonNamesFormattingAttributes {
    pub fn bit_value(&self) -> PersonNamesFormattingAttributesMask {
        match self {
            PersonNamesFormattingAttributes::GivenFirst => 1 << 0,
            PersonNamesFormattingAttributes::SurnameFirst => 1 << 1,
            PersonNamesFormattingAttributes::Sorting => 1 << 2,
            PersonNamesFormattingAttributes::Short => 1 << 3,
            PersonNamesFormattingAttributes::Medium => 1 << 4,
            PersonNamesFormattingAttributes::Long => 1 << 5,
            PersonNamesFormattingAttributes::Addressing => 1 << 6,
            PersonNamesFormattingAttributes::Referring => 1 << 7,
            PersonNamesFormattingAttributes::Monogram => 1 << 8,
            PersonNamesFormattingAttributes::Formal => 1 << 9,
            PersonNamesFormattingAttributes::Informal => 1 << 10,
        }
    }
}

impl From<FormattingOrder> for PersonNamesFormattingAttributes {
    fn from(value: FormattingOrder) -> Self {
        match value {
            FormattingOrder::GivenFirst => PersonNamesFormattingAttributes::GivenFirst,
            FormattingOrder::SurnameFirst => PersonNamesFormattingAttributes::SurnameFirst,
            FormattingOrder::Sorting => PersonNamesFormattingAttributes::Sorting,
        }
    }
}

impl From<FormattingFormality> for PersonNamesFormattingAttributes {
    fn from(value: FormattingFormality) -> Self {
        match value {
            FormattingFormality::Formal => PersonNamesFormattingAttributes::Formal,
            FormattingFormality::Informal => PersonNamesFormattingAttributes::Informal,
        }
    }
}

impl From<FormattingLength> for PersonNamesFormattingAttributes {
    fn from(value: FormattingLength) -> Self {
        match value {
            FormattingLength::Short => PersonNamesFormattingAttributes::Short,
            FormattingLength::Medium => PersonNamesFormattingAttributes::Medium,
            FormattingLength::Long => PersonNamesFormattingAttributes::Long,
        }
    }
}

impl From<FormattingUsage> for PersonNamesFormattingAttributes {
    fn from(value: FormattingUsage) -> Self {
        match value {
            FormattingUsage::Addressing => PersonNamesFormattingAttributes::Addressing,
            FormattingUsage::Referring => PersonNamesFormattingAttributes::Referring,
            FormattingUsage::Monogram => PersonNamesFormattingAttributes::Monogram,
        }
    }
}

pub type PersonNamesFormattingAttributesMask = u32;

/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#personname-element>
#[zerovec::make_varule(PersonNamesFormattingDataVarULE)]
#[zerovec::skip_derive(ZeroMapKV, Ord)]
#[zerovec::derive(Debug)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::personnames::provider))]
#[cfg_attr(feature = "datagen", zerovec::derive(Serialize))]
#[cfg_attr(
    feature = "serde",
    derive(serde::Deserialize),
    zerovec::derive(Deserialize)
)]
pub struct PersonNamesFormattingData<'data> {
    /// Attributes
    /// <https://www.unicode.org/reports/tr35/tr35-personNames.html#personname-element>
    pub attributes: PersonNamesFormattingAttributesMask,
    /// <https://www.unicode.org/reports/tr35/tr35-personNames.html#namepattern-syntax>
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub patterns: VarZeroVec<'data, str>,
}

impl Debug for PersonNamesFormattingData<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        let mut debug = f.debug_struct("PersonNamesFormattingData");

        debug.field(
            "given_first",
            &(self.attributes & PersonNamesFormattingAttributes::GivenFirst.bit_value()
                == PersonNamesFormattingAttributes::GivenFirst.bit_value()),
        );
        debug.field(
            "surname_first",
            &(self.attributes & PersonNamesFormattingAttributes::SurnameFirst.bit_value()
                == PersonNamesFormattingAttributes::SurnameFirst.bit_value()),
        );
        debug.field(
            "sorting",
            &(self.attributes & PersonNamesFormattingAttributes::Sorting.bit_value()
                == PersonNamesFormattingAttributes::Sorting.bit_value()),
        );
        debug.field(
            "short",
            &(self.attributes & PersonNamesFormattingAttributes::Short.bit_value()
                == PersonNamesFormattingAttributes::Short.bit_value()),
        );
        debug.field(
            "medium",
            &(self.attributes & PersonNamesFormattingAttributes::Medium.bit_value()
                == PersonNamesFormattingAttributes::Medium.bit_value()),
        );
        debug.field(
            "long",
            &(self.attributes & PersonNamesFormattingAttributes::Long.bit_value()
                == PersonNamesFormattingAttributes::Long.bit_value()),
        );
        debug.field(
            "addressing",
            &(self.attributes & PersonNamesFormattingAttributes::Addressing.bit_value()
                == PersonNamesFormattingAttributes::Addressing.bit_value()),
        );
        debug.field(
            "referring",
            &(self.attributes & PersonNamesFormattingAttributes::Referring.bit_value()
                == PersonNamesFormattingAttributes::Referring.bit_value()),
        );
        debug.field(
            "monogram",
            &(self.attributes & PersonNamesFormattingAttributes::Monogram.bit_value()
                == PersonNamesFormattingAttributes::Monogram.bit_value()),
        );
        debug.field(
            "formal",
            &(self.attributes & PersonNamesFormattingAttributes::Formal.bit_value()
                == PersonNamesFormattingAttributes::Formal.bit_value()),
        );
        debug.field(
            "informal",
            &(self.attributes & PersonNamesFormattingAttributes::Informal.bit_value()
                == PersonNamesFormattingAttributes::Informal.bit_value()),
        );
        debug.finish()
    }
}
