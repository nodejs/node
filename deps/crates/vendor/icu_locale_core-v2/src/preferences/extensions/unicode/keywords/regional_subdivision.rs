// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::preferences::extensions::unicode::errors::PreferencesParseError;
use crate::preferences::extensions::unicode::struct_keyword;
use crate::{
    extensions::unicode::{SubdivisionId, Value},
    subtags::Subtag,
};

struct_keyword!(
    /// A Unicode Subdivision Identifier defines a regional subdivision used for locales.
    ///
    /// The valid values are listed in [LDML](https://unicode.org/reports/tr35/#UnicodeSubdivisionIdentifier).
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::locale;
    /// use icu::locale::extensions::unicode::key;
    /// use icu::locale::preferences::extensions::unicode::keywords::RegionalSubdivision;
    /// use writeable::assert_writeable_eq;
    ///
    /// // Texan English
    /// let locale = locale!("en-US-u-sd-ustx");
    ///
    /// let normal_region = locale.id.region.unwrap();
    /// let sd_extension_value = locale.extensions.unicode.keywords.get(&key!("sd")).unwrap();
    /// let regional_subdivision = RegionalSubdivision::try_from(sd_extension_value.clone()).unwrap();
    ///
    /// assert_writeable_eq!(normal_region, "US");
    /// assert_writeable_eq!(regional_subdivision.region, "US");
    /// assert_writeable_eq!(regional_subdivision.suffix, "tx");
    /// ```
    ///
    /// The subdivision "true" is not supported; see [CLDR-19163](https://unicode-org.atlassian.net/browse/CLDR-19163):
    ///
    /// ```
    /// use icu::locale::extensions::unicode::value;
    /// use icu::locale::preferences::extensions::unicode::keywords::RegionalSubdivision;
    /// use icu::locale::preferences::extensions::unicode::errors::PreferencesParseError;
    /// use writeable::assert_writeable_eq;
    ///
    /// assert!(matches!(
    ///     RegionalSubdivision::try_from(value!("true")),
    ///     Err(PreferencesParseError::InvalidKeywordValue),
    /// ));
    /// ```
    [Copy]
    RegionalSubdivision,
    "sd",
    SubdivisionId,
    |input: &Value| {
        input
            .as_single_subtag()
            .and_then(|subtag| subtag.as_str().parse().ok().map(Self))
            .ok_or(PreferencesParseError::InvalidKeywordValue)
    },
    |input: &RegionalSubdivision| {
        Value::from_subtag(Some(input.0.into_subtag()))
    }
);

#[cfg(test)]
mod test {
    use super::*;
    use crate::extensions::unicode;
    use crate::extensions::unicode::subdivision_suffix;
    use crate::subtags::region;

    #[test]
    fn region_subdivision_test() {
        let val = unicode::value!("uksct");
        let rg: RegionalSubdivision = val.try_into().unwrap();
        assert_eq!(rg.region, region!("UK"));
        assert_eq!(rg.suffix, subdivision_suffix!("sct"));

        for i in &["4aabel", "a4bel", "ukabcde"] {
            let val = Value::try_from_str(i).unwrap();
            let rg: Result<RegionalSubdivision, _> = val.try_into();
            assert!(rg.is_err());
        }
    }
}
