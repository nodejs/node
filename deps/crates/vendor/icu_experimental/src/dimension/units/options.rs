// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options for [`UnitsFormatter`](crate::dimension::units::formatter::UnitsFormatter).

/// A collection of configuration options that determine the formatting behavior of
/// [`UnitsFormatter`](crate::dimension::units::formatter::UnitsFormatter).
#[derive(Copy, Debug, Eq, PartialEq, Clone, Default)]
#[non_exhaustive]
pub struct UnitsFormatterOptions {
    /// The width of the units format.
    pub width: Width,
}

impl From<Width> for UnitsFormatterOptions {
    fn from(width: Width) -> Self {
        Self { width }
    }
}

#[derive(Debug, Eq, PartialEq, Clone, Copy, Default)]
#[non_exhaustive]
pub enum Width {
    /// Format the units with the long units format.
    ///
    /// For example, 1 hour formats as "1 hour" in en-US.
    Long,

    /// Format the units with the short units format.
    ///
    /// For example, 1 hour formats as "1 hr" in en-US.
    #[default]
    Short,

    /// Format the units with the narrow units format.
    ///
    /// The narrow symbol may be ambiguous, so it should be evident from context which
    /// units is being represented.
    ///
    /// For example, 1 hour formats as "1 h" in most locales.
    Narrow,
}

impl From<Width> for tinystr::TinyStr8 {
    fn from(width: Width) -> Self {
        match width {
            Width::Long => "long".parse().unwrap(),
            Width::Short => "short".parse().unwrap(),
            Width::Narrow => "narrow".parse().unwrap(),
        }
    }
}
