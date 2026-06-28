// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use criterion::{criterion_group, criterion_main, Criterion};

use icu_plurals::{PluralRuleType, PluralRules};

fn pluralrules(c: &mut Criterion) {
    let plurals_data =
        serde_json::from_str::<fixtures::PluralsFixture>(include_str!("fixtures/plurals.json"))
            .expect("Failed to read a fixture");
    let numbers_data =
        serde_json::from_str::<fixtures::NumbersFixture>(include_str!("fixtures/numbers.json"))
            .expect("Failed to read a fixture");

    c.bench_function("plurals/pluralrules/overview", |b| {
        b.iter(|| {
            for lang in &plurals_data.langs {
                let pr =
                    PluralRules::try_new(lang.into(), PluralRuleType::Cardinal.into()).unwrap();
                for s in &numbers_data.usize {
                    let _ = pr.category_for(*s);
                }
            }
        })
    });

    {
        use criterion::black_box;
        use icu_locale_core::locale;

        c.bench_function("plurals/pluralrules/construct/fs", |b| {
            b.iter(|| {
                for lang in &plurals_data.langs {
                    PluralRules::try_new(lang.into(), PluralRuleType::Ordinal.into()).unwrap();
                    PluralRules::try_new(lang.into(), PluralRuleType::Cardinal.into()).unwrap();
                }
            });
        });

        let pr =
            PluralRules::try_new(locale!("ru").into(), PluralRuleType::Cardinal.into()).unwrap();
        c.bench_function("plurals/pluralrules/select/fs", |b| {
            b.iter(|| {
                for s in &numbers_data.usize {
                    let _ = pr.category_for(black_box(*s));
                }
            })
        });
    }
}

criterion_group!(benches, pluralrules,);
criterion_main!(benches);
