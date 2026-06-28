// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options for [`PercentFormatter`](crate::dimension::percent::formatter::PercentFormatter).

/// A collection of configuration options that determine the formatting behavior of
/// [`PercentFormatter`](crate::dimension::percent::formatter::PercentFormatter).
#[derive(Copy, Debug, Eq, PartialEq, Clone, Default)]
#[non_exhaustive]
pub struct PercentFormatterOptions {
    /// The display of the percent format.
    pub display: Display,
}

impl From<Display> for PercentFormatterOptions {
    fn from(display: Display) -> Self {
        Self { display }
    }
}

#[derive(Default, Debug, Eq, PartialEq, Clone, Copy)]
#[non_exhaustive]
pub enum Display {
    /// Format the Percent to display with the standard formatting for the locale.
    ///
    /// For example 123 -> 123% in en-US.
    #[default]
    Standard,
    /// Format the Percent to display as an approximate value.
    ///
    /// For example 123 -> ~123% in en-US.
    Approximate,
    /// Format the Percent to display with an explicit sign.
    ///
    /// For example 123 -> +123% in en-US.
    ExplicitSign,
}
