// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_locale_core::locale;
use icu_plurals::{PluralCategory, PluralOperands, PluralRulesWithRanges};

#[test]
fn test_plural_ranges_raw() {
    assert_eq!(
        PluralRulesWithRanges::try_new_cardinal(locale!("he").into())
            .unwrap()
            .resolve_range(PluralCategory::One, PluralCategory::Two),
        PluralCategory::Other
    );
}

#[test]
fn test_plural_ranges_optimized_data() {
    assert_eq!(
        PluralRulesWithRanges::try_new_ordinal(locale!("en").into())
            .unwrap()
            .resolve_range(PluralCategory::One, PluralCategory::Other),
        PluralCategory::Other
    );
}

#[test]
fn test_plural_ranges_missing_data_fallback() {
    assert_eq!(
        PluralRulesWithRanges::try_new_cardinal(locale!("nl").into())
            .unwrap()
            .resolve_range(PluralCategory::Two, PluralCategory::Many),
        PluralCategory::Many
    );
}

#[test]
fn test_plural_ranges_full() {
    let ranges = PluralRulesWithRanges::try_new(locale!("sl").into(), Default::default()).unwrap();
    let start: PluralOperands = "0.5".parse().unwrap(); // PluralCategory::Other
    let end: PluralOperands = PluralOperands::from(1); // PluralCategory::One

    assert_eq!(ranges.category_for_range(start, end), PluralCategory::Few)
}
