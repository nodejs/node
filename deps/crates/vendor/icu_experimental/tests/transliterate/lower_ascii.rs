// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::any::TypeId;

use icu_experimental::transliterate::{
    provider::TransliteratorRulesV1, RuleCollection, RuleCollectionProvider, Transliterator,
};
use icu_provider::prelude::*;

struct TransliteratorMultiSourceProvider<'a>(
    RuleCollectionProvider<
        'a,
        icu_properties::provider::Baked,
        icu_normalizer::provider::Baked,
        icu_casemap::provider::Baked,
    >,
);

impl<'a, M> DataProvider<M> for TransliteratorMultiSourceProvider<'a>
where
    M: DataMarker,
    RuleCollectionProvider<
        'a,
        icu_properties::provider::Baked,
        icu_normalizer::provider::Baked,
        icu_casemap::provider::Baked,
    >: DataProvider<M>,
{
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        if TypeId::of::<M>() == TypeId::of::<TransliteratorRulesV1>() {
            let mut silent_req = req;
            silent_req.metadata.silent = true;
            if let Some(response) = DataProvider::<TransliteratorRulesV1>::load(
                &icu_experimental::provider::Baked,
                silent_req,
            )
            .allow_identifier_not_found()?
            {
                return Ok(DataResponse {
                    metadata: response.metadata,
                    payload: response.payload.dynamic_cast()?,
                });
            }
        }
        self.0.load(req)
    }
}

#[test]
fn test_lower_ascii() {
    let mut collection = RuleCollection::default();
    // Register Latin-ASCII so that the alias mapping gets added
    collection.register_source(
        &"und-t-und-latn-d0-ascii".parse().unwrap(),
        "<unused>".to_string(),
        ["Latin-ASCII"],
        false,
        true,
    );
    // Now register our new transliterator
    collection.register_source(
        &"und-t-und-x0-lowascii".parse().unwrap(),
        "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; ::Latin-ASCII;".to_string(),
        [],
        false,
        true,
    );
    let provider = TransliteratorMultiSourceProvider(collection.as_provider());
    let t = Transliterator::try_new_unstable(
        &provider,
        &provider,
        &provider,
        &"und-t-und-x0-lowascii".parse().unwrap(),
    )
    .unwrap();
    let r = t.transliterate("ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ".to_string());
    assert_eq!(r, "internationalization");
}
