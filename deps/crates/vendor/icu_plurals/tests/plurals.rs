// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_locale_core::{langid, locale};
use icu_plurals::{provider::PluralsCardinalV1, PluralCategory, PluralRules};
use icu_provider::prelude::*;

#[test]
fn test_plural_rules() {
    assert_eq!(
        PluralRules::try_new(locale!("en").into(), Default::default())
            .unwrap()
            .category_for(5_usize),
        PluralCategory::Other
    );
}

#[test]
fn test_static_load_works() {
    DataProvider::<PluralsCardinalV1>::load(
        &icu_plurals::provider::Baked,
        DataRequest {
            id: DataIdentifierBorrowed::for_locale(&langid!("en").into()),
            ..Default::default()
        },
    )
    .expect("Failed to load payload");
}

#[test]
fn test_plural_category_all() {
    let categories: Vec<PluralCategory> = PluralCategory::all().collect();

    assert_eq!(categories.len(), 6);

    assert_eq!(categories[0], PluralCategory::Few);
    assert_eq!(categories[1], PluralCategory::Many);
    assert_eq!(categories[2], PluralCategory::One);
    assert_eq!(categories[3], PluralCategory::Other);
    assert_eq!(categories[4], PluralCategory::Two);
    assert_eq!(categories[5], PluralCategory::Zero);
}
