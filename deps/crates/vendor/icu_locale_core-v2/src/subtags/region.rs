// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

impl_tinystr_subtag!(
    /// A region subtag (examples: `"US"`, `"CN"`, `"001"` etc.)
    ///
    /// [`Region`] represents a Unicode region code conformant to the
    /// [`unicode_region_subtag`] field of the Language and Locale Identifier,
    /// i.e. an [ISO 3166-1 alpha 2] or [UN M.49] (macro regions only) value.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::subtags::Region;
    ///
    /// let region: Region =
    ///     "DE".parse().expect("Failed to parse a region subtag.");
    /// ```
    ///
    /// [`unicode_region_subtag`]: https://unicode.org/reports/tr35/#unicode_region_subtag_validity
    /// [ISO 3166-1 alpha 2]: https://en.wikipedia.org/wiki/ISO_3166-1
    /// [UN M.49]: https://en.wikipedia.org/wiki/UN_M49
    Region,
    subtags,
    region,
    subtags_region,
    2..=3,
    s,
    if s.len() == 2 {
        s.is_ascii_alphabetic()
    } else {
        s.is_ascii_numeric()
    },
    if s.len() == 2 {
        s.to_ascii_uppercase()
    } else {
        s
    },
    if s.len() == 2 {
        s.is_ascii_alphabetic_uppercase()
    } else {
        s.is_ascii_numeric()
    },
    InvalidSubtag,
    ["FR", "009"],
    ["12", "FRA", "b2"],
);

impl Region {
    /// Returns true if the Region has an alphabetic code.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::subtags::region;
    ///
    /// assert!(region!("us").is_alphabetic());
    /// ```
    pub fn is_alphabetic(&self) -> bool {
        self.0.len() == 2
    }
}
