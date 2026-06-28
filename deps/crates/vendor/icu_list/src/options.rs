// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options used by types in this crate

/// A list of options set by the developer to adjust the behavior of the [`ListFormatter`](super::ListFormatter).
///
/// # Examples
/// ```
/// use icu::list::options::{ListFormatterOptions, ListLength};
///
/// let options = ListFormatterOptions::default().with_length(ListLength::Wide);
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub struct ListFormatterOptions {
    /// The length variant should reflect available space for the list.
    pub length: Option<ListLength>,
}

impl Default for ListFormatterOptions {
    fn default() -> Self {
        Self::default()
    }
}

impl ListFormatterOptions {
    /// Constructs a new [`ListFormatterOptions`] struct.
    pub const fn default() -> Self {
        Self { length: None }
    }

    /// Auguments the struct with the set [`ListLength`].
    pub const fn with_length(mut self, length: ListLength) -> Self {
        self.length = Some(length);
        self
    }
}

/// Represents the style of a list. See the
/// [CLDR spec](https://unicode.org/reports/tr35/tr35-general.html#ListPatterns)
/// for an explanation of the different styles.
#[derive(Copy, Clone, PartialEq, Eq, Debug, Hash, Default)]
#[non_exhaustive]
pub enum ListLength {
    /// A typical list
    #[default]
    Wide,
    /// A shorter list
    Short,
    /// The shortest type of list
    Narrow,
    // *Important*: When adding a variant here, make sure the code in
    // ListFormatterPatterns::{start, middle, end, pair} stays panic-free!
}
