// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::string::String;
use alloc::vec::Vec;
use core::fmt;
use displaydoc::Display;
use icu_locale_core::Locale;
use icu_provider::DataError;

/// Trait for providing person name data.
pub trait PersonName {
    /// Returns the name locale of person name.
    fn name_locale(&self) -> Option<&Locale>;

    /// Returns the preferred order of person name.
    fn preferred_order(&self) -> Option<&PreferredOrder>;

    /// Returns the value of the given field name, it *must* match the name field requested.
    /// The string should be in NFC.
    fn get(&self, field: NameField) -> &str;

    /// Returns all available name field.
    fn available_name_fields(&self) -> Vec<NameField>;

    /// Returns true if the provided field name is available.
    fn has_name_field_kind(&self, lookup_name_field: NameFieldKind) -> bool;

    /// Returns true if person have the name field matching the type and modifier.
    fn has_name_field(&self, lookup_name_field: NameField) -> bool;
}

///
/// Error handling for the person name formatter.
#[derive(Clone, Eq, PartialEq, Debug, Display)]
pub enum PersonNamesFormatterError {
    #[displaydoc("{0}")]
    ParseError(String),
    #[displaydoc("Invalid person name")]
    InvalidPersonName,
    #[displaydoc("Invalid person name")]
    InvalidLocale,
    #[displaydoc("Invalid CLDR data")]
    InvalidCldrData,
    #[displaydoc("{0}")]
    Data(DataError),
    #[displaydoc("{0}")]
    Pattern(icu_pattern::Error),
}

impl core::error::Error for PersonNamesFormatterError {}

impl From<DataError> for PersonNamesFormatterError {
    fn from(e: DataError) -> Self {
        PersonNamesFormatterError::Data(e)
    }
}

impl From<icu_pattern::Error> for PersonNamesFormatterError {
    fn from(e: icu_pattern::Error) -> Self {
        PersonNamesFormatterError::Pattern(e)
    }
}

/// Field Modifiers.
///
/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#modifiers>
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub(crate) enum FieldModifier {
    None,
    Informal,
    Prefix,
    Core,
    AllCaps,
    InitialCap,
    Initial,
    Monogram,
}

impl FieldModifier {
    pub(crate) fn bit_value(self) -> u32 {
        match &self {
            FieldModifier::None => 0,
            FieldModifier::Informal => 1 << 0,
            FieldModifier::Prefix => 1 << 1,
            FieldModifier::Core => 1 << 2,
            FieldModifier::AllCaps => 1 << 3,
            FieldModifier::InitialCap => 1 << 4,
            FieldModifier::Initial => 1 << 5,
            FieldModifier::Monogram => 1 << 6,
        }
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FieldCapsStyle {
    Auto,
    AllCaps,
    InitialCap,
}

impl From<FieldCapsStyle> for FieldModifier {
    fn from(value: FieldCapsStyle) -> Self {
        match value {
            FieldCapsStyle::Auto => FieldModifier::None,
            FieldCapsStyle::AllCaps => FieldModifier::AllCaps,
            FieldCapsStyle::InitialCap => FieldModifier::InitialCap,
        }
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FieldPart {
    Auto,
    Core,
    Prefix,
}

impl From<FieldPart> for FieldModifier {
    fn from(value: FieldPart) -> Self {
        match value {
            FieldPart::Auto => FieldModifier::None,
            FieldPart::Core => FieldModifier::Core,
            FieldPart::Prefix => FieldModifier::Prefix,
        }
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FieldLength {
    Auto,
    Initial,
    Monogram,
}

impl From<FieldLength> for FieldModifier {
    fn from(value: FieldLength) -> Self {
        match value {
            FieldLength::Auto => FieldModifier::None,
            FieldLength::Initial => FieldModifier::Initial,
            FieldLength::Monogram => FieldModifier::Monogram,
        }
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FieldFormality {
    Auto,
    Informal,
}

impl From<FieldFormality> for FieldModifier {
    fn from(value: FieldFormality) -> Self {
        match value {
            FieldFormality::Auto => FieldModifier::None,
            FieldFormality::Informal => FieldModifier::Informal,
        }
    }
}

/// Field Modifiers Set. (must be the same as `FieldModifier` repr)
#[derive(Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Copy)]
pub struct FieldModifierSet {
    pub(crate) value: u32,
}

impl FieldModifierSet {
    /// Return true of the field modifier is set. (None will always return true.)
    pub(crate) fn has_field(self, modifier: FieldModifier) -> bool {
        self.value & modifier.bit_value() == modifier.bit_value()
    }

    /// Returns a copy of the field modifier set with the part changed.
    pub(crate) fn with_part(self, part: FieldPart) -> FieldModifierSet {
        FieldModifierSet {
            value: (self.value
                & (u32::MAX
                    ^ (FieldModifier::Core.bit_value() | FieldModifier::Prefix.bit_value())))
                | FieldModifier::from(part).bit_value(),
        }
    }
    /// Returns a copy of the field modifier set with the length changed.
    pub(crate) fn with_length(self, length: FieldLength) -> FieldModifierSet {
        FieldModifierSet {
            value: (self.value
                & (u32::MAX
                    ^ (FieldModifier::Monogram.bit_value() | FieldModifier::Initial.bit_value())))
                | FieldModifier::from(length).bit_value(),
        }
    }

    pub fn formality(formality: FieldFormality) -> Self {
        Self::new(
            FieldCapsStyle::Auto,
            FieldPart::Auto,
            FieldLength::Auto,
            formality,
        )
    }
    pub fn style(style: FieldCapsStyle) -> Self {
        Self::new(
            style,
            FieldPart::Auto,
            FieldLength::Auto,
            FieldFormality::Auto,
        )
    }
    pub fn part(part: FieldPart) -> Self {
        Self::new(
            FieldCapsStyle::Auto,
            part,
            FieldLength::Auto,
            FieldFormality::Auto,
        )
    }
    pub fn length(length: FieldLength) -> Self {
        Self::new(
            FieldCapsStyle::Auto,
            FieldPart::Auto,
            length,
            FieldFormality::Auto,
        )
    }

    pub fn new(
        style: FieldCapsStyle,
        part: FieldPart,
        length: FieldLength,
        formality: FieldFormality,
    ) -> Self {
        FieldModifierSet {
            value: FieldModifier::from(style).bit_value()
                | FieldModifier::from(part).bit_value()
                | FieldModifier::from(length).bit_value()
                | FieldModifier::from(formality).bit_value(),
        }
    }
}

impl Default for FieldModifierSet {
    fn default() -> Self {
        Self::new(
            FieldCapsStyle::Auto,
            FieldPart::Auto,
            FieldLength::Auto,
            FieldFormality::Auto,
        )
    }
}

impl fmt::Debug for FieldModifierSet {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut debug = f.debug_struct("FieldModifierSet");
        debug.field("core", &self.has_field(FieldModifier::Core));
        debug.field("informal", &self.has_field(FieldModifier::Informal));
        debug.field("monogram", &self.has_field(FieldModifier::Monogram));
        debug.field("initial", &self.has_field(FieldModifier::Initial));
        debug.field("prefix", &self.has_field(FieldModifier::Prefix));
        debug.field("all_caps", &self.has_field(FieldModifier::AllCaps));
        debug.field("initial_cap", &self.has_field(FieldModifier::InitialCap));
        debug.finish()
    }
}

/// Name Fields defined by Unicode specifications.
///
/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#fields>
#[derive(Eq, Ord, PartialOrd, PartialEq, Hash, Debug, Clone, Copy)]
pub struct NameField {
    pub kind: NameFieldKind,
    pub modifier: FieldModifierSet,
}

#[derive(Eq, Ord, PartialOrd, PartialEq, Hash, Debug, Clone, Copy)]
pub enum NameFieldKind {
    Title,
    Given,
    Given2,
    Surname,
    Surname2,
    Generation,
    Credentials,
}

/// An enum to specify the preferred field order for the name.
///
/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#person-name-object>
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum PreferredOrder {
    Default,
    GivenFirst,
    SurnameFirst,
}

/// Formatting Order
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FormattingOrder {
    GivenFirst,
    SurnameFirst,
    Sorting,
}

/// Formatting Length
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FormattingLength {
    Short,
    Medium,
    Long,
}

/// Formatting Usage
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FormattingUsage {
    Addressing,
    Referring,
    Monogram,
}

/// Formatting Formality
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FormattingFormality {
    Formal,
    Informal,
}

/// Person name formatter options.
#[derive(Clone, Eq, PartialEq, Debug)]
pub struct PersonNamesFormatterOptions {
    // TODO: the target locale should be maximized when passed into the formatter.
    pub target_locale: Locale,
    pub order: FormattingOrder,
    pub length: FormattingLength,
    pub usage: FormattingUsage,
    pub formality: FormattingFormality,
}

impl PersonNamesFormatterOptions {
    // TODO: Remove this function. It depends on compiled_data for the LocaleExpander.
    #[cfg(feature = "compiled_data")]
    pub fn new(
        target_locale: Locale,
        order: FormattingOrder,
        length: FormattingLength,
        usage: FormattingUsage,
        formality: FormattingFormality,
    ) -> Self {
        let lc = icu_locale::LocaleExpander::new_extended();
        let mut final_locale = target_locale.clone();
        lc.maximize(&mut final_locale.id);
        Self {
            target_locale: final_locale,
            order,
            length,
            usage,
            formality,
        }
    }
}
