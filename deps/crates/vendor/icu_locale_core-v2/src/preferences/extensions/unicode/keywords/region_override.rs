// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::extensions::unicode::{SubdivisionId, Value};
use crate::preferences::extensions::unicode::errors::PreferencesParseError;
use crate::preferences::extensions::unicode::struct_keyword;

struct_keyword!(
    /// A Region Override specifies an alternate region to use for obtaining certain region-specific default values.
    ///
    /// The valid values are listed in [LDML](https://unicode.org/reports/tr35/#RegionOverride).
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::locale;
    /// use icu::locale::extensions::unicode::key;
    /// use icu::locale::preferences::extensions::unicode::keywords::RegionOverride;
    /// use writeable::assert_writeable_eq;
    ///
    /// // American English with British user preferences
    /// let locale = locale!("en-US-u-rg-gbzzzz");
    ///
    /// let normal_region = locale.id.region.unwrap();
    /// let rg_extension_value = locale.extensions.unicode.keywords.get(&key!("rg")).unwrap();
    /// let region_override = RegionOverride::try_from(rg_extension_value.clone()).unwrap();
    ///
    /// assert_writeable_eq!(normal_region, "US");
    /// assert_writeable_eq!(region_override.region, "GB");
    /// assert_writeable_eq!(region_override.suffix, "zzzz");
    /// ```
    [Copy]
    RegionOverride,
    "rg",
    SubdivisionId,
    |input: &Value| {
        input
            .as_single_subtag()
            .and_then(|subtag| subtag.as_str().parse().ok().map(Self))
            .ok_or(PreferencesParseError::InvalidKeywordValue)
    },
    |input: &RegionOverride| {
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
    fn region_override_test() {
        let val = unicode::value!("uksct");
        let rg: RegionOverride = val.try_into().unwrap();
        assert_eq!(rg.0.region, region!("UK"));
        assert_eq!(rg.0.suffix, subdivision_suffix!("sct"));

        let val = unicode::value!("usca");
        let rg: RegionOverride = val.try_into().unwrap();
        assert_eq!(rg.0.region, region!("US"));
        assert_eq!(rg.0.suffix, subdivision_suffix!("ca"));

        let val = unicode::value!("419bel");
        let rg: RegionOverride = val.try_into().unwrap();
        assert_eq!(rg.0.region, region!("419"));
        assert_eq!(rg.0.suffix, subdivision_suffix!("bel"));

        let val = unicode::value!("uszzzz");
        let rg: RegionOverride = val.try_into().unwrap();
        assert_eq!(rg.0.region, region!("us"));
        assert_eq!(rg.0.suffix, subdivision_suffix!("zzzz"));

        for i in &["4aabel", "a4bel", "ukabcde"] {
            let val = Value::try_from_str(i).unwrap();
            let rg: Result<RegionOverride, _> = val.try_into();
            assert!(rg.is_err());
        }
    }
}
