// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options for [`DisplayNames`](crate::DisplayNames).

/// A bag of options defining how region or language codes will be translated.
///
/// # Example
///
/// ```
/// use icu::experimental::displaynames::{
///     DisplayNamesOptions, multi::RegionDisplayNames, Style,
/// };
/// use icu::locale::{locale, subtags::region};
///
/// let locale = locale!("en-001");
/// let mut options: DisplayNamesOptions = Default::default();
/// options.style = Some(Style::Short);
/// let display_name = RegionDisplayNames::try_new(locale.into(), options)
///     .expect("Data should load successfully");
///
/// // Full name would be "Bosnia & Herzegovina"
/// assert_eq!(display_name.of(region!("BA")), Some("Bosnia"));
/// ```
#[derive(Copy, Debug, Eq, PartialEq, Clone, Default)]
#[non_exhaustive]
pub struct DisplayNamesOptions {
    /// The optional formatting style to use for display name.
    pub style: Option<Style>,
    /// The fallback return when the system does not have the
    /// requested display name, defaults to "code".
    pub fallback: Fallback,
    /// The language display kind, defaults to "dialect".
    pub language_display: LanguageDisplay,
}

/// An enum for formatting style.
#[allow(missing_docs)] // The variants are self explanatory.
#[non_exhaustive]
#[derive(Debug, Eq, PartialEq, Clone, Copy)]
pub enum Style {
    Narrow,
    Short,
    Long,
    Menu,
}

/// An enum for fallback return when the system does not have the
/// requested display name.
#[allow(missing_docs)]
#[non_exhaustive]
#[derive(Debug, Default, Eq, PartialEq, Clone, Copy)]
pub enum Fallback {
    /// Fall back to the BCP-47 code when display name cannot be found
    #[default]
    Code,
    /// Do not fall back, return an error when the display name cannot be found
    None,
}

/// An enum for the language display kind.
#[allow(missing_docs)] // The variants are self explanatory.
#[non_exhaustive]
#[derive(Debug, Default, Eq, PartialEq, Clone, Copy)]
pub enum LanguageDisplay {
    #[default]
    Dialect,
    Standard,
}
