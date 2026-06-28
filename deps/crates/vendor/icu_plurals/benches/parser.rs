// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use criterion::{black_box, criterion_group, criterion_main, Criterion};

use icu_provider::prelude::*;

fn parser(c: &mut Criterion) {
    use icu_plurals::provider::rules::reference::parse_condition;

    let fixture_data =
        serde_json::from_str::<fixtures::PluralsFixture>(include_str!("fixtures/plurals.json"))
            .expect("Failed to read a fixture");

    let mut rules = vec![];

    for langid in fixture_data.langs {
        let response: DataResponse<icu_plurals::provider::PluralsCardinalV1> =
            icu_plurals::provider::Baked
                .load(DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&langid.into()),
                    ..Default::default()
                })
                .unwrap();
        let data = response.payload.get();

        let r = [
            data.zero.as_ref(),
            data.one.as_ref(),
            data.two.as_ref(),
            data.few.as_ref(),
            data.many.as_ref(),
        ];

        for x in r.iter().flatten() {
            rules.push(x.to_string());
        }
    }

    c.bench_function("plurals/parser/overview", |b| {
        b.iter(|| {
            for rule in &rules {
                let _ = parse_condition(black_box(rule.as_bytes()));
            }
        })
    });

    c.bench_function("plurals/parser/lex", |b| {
        use icu_plurals::provider::rules::reference::Lexer;
        b.iter(|| {
            for rule in &rules {
                let _ = Lexer::new(black_box(rule.as_bytes())).count();
            }
        })
    });
}

criterion_group!(benches, parser,);
criterion_main!(benches);
