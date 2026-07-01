// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use icu_casemap::CaseMapper;
use icu_locale_core::langid;
use icu_normalizer::DecomposingNormalizerBorrowed;

const TEST_STRING_EN: &str = "One of the key design principles of ICU4X is to make locale data small and portable, allowing it to be pulled from multiple sources depending on the needs of the application.  This document explains how that goal can be achieved.";

// First 50 lines of the Iliad, in precomposed Greek
// (The Iliad is thousands of years old and public domain)
// Sources can be found in https://www.perseus.tufts.edu/hopper/text?doc=Perseus:text:1999.01.0133 or https://www.sacred-texts.com/cla/homer/greek/ili01.htm
const ILIAD: &str = include_str!("data/Iliad.txt");

fn overview_bench(c: &mut Criterion) {
    let casemapper = CaseMapper::new();
    let root = langid!("und");
    let tr = langid!("tr");
    c.bench_function("icu_casemap/overview", |b| {
        b.iter(|| {
            black_box(casemapper.lowercase_to_string(black_box(TEST_STRING_EN), &root));
            black_box(casemapper.uppercase_to_string(black_box(TEST_STRING_EN), &root));
        });
    });

    c.bench_function("icu_casemap/titlecase_segment", |b| {
        b.iter(|| {
            for s in TEST_STRING_EN.split(' ') {
                black_box(casemapper.titlecase_segment_with_only_case_data_to_string(
                    black_box(s),
                    &root,
                    Default::default(),
                ));
            }
        });
    });

    c.bench_function("icu_casemap/folding", |b| {
        b.iter(|| {
            black_box(casemapper.fold_string(black_box(TEST_STRING_EN)));
        });
    });
    c.bench_function("icu_casemap/uppercase_tr", |b| {
        b.iter(|| {
            black_box(casemapper.uppercase_to_string(black_box(TEST_STRING_EN), &tr));
        });
    });
}
fn greek_uppercasing(_c: &mut Criterion) {
    {
        let c = _c;
        let casemapper = CaseMapper::new();
        let root = langid!("und");
        let el = langid!("el");

        let iliad_lowercase = casemapper.lowercase_to_string(ILIAD, &root);
        let decomposer = DecomposingNormalizerBorrowed::new_nfd();
        let nfd = decomposer.normalize_utf8(ILIAD.as_bytes());
        let nfd_lowercase = decomposer.normalize_utf8(iliad_lowercase.as_bytes());

        let mut group =
            c.benchmark_group("icu_casemap/greek_uppercasing/precomposed/upper_from_title");
        group.bench_function("root", |b| {
            b.iter(|| {
                black_box(casemapper.uppercase_to_string(black_box(ILIAD), &root));
            });
        });
        group.bench_function("greek", |b| {
            b.iter(|| {
                black_box(casemapper.uppercase_to_string(black_box(ILIAD), &el));
            });
        });
        group.finish();

        let mut group =
            c.benchmark_group("icu_casemap/greek_uppercasing/precomposed/upper_from_lower");
        group.bench_function("root", |b| {
            b.iter(|| {
                black_box(casemapper.uppercase_to_string(black_box(&iliad_lowercase), &root));
            });
        });
        group.bench_function("greek", |b| {
            b.iter(|| {
                black_box(casemapper.uppercase_to_string(black_box(&iliad_lowercase), &el));
            });
        });
        group.finish();

        let mut group =
            c.benchmark_group("icu_casemap/greek_uppercasing/decomposed/upper_from_title");
        group.bench_function("root", |b| {
            b.iter(|| {
                black_box(casemapper.uppercase_to_string(black_box(&nfd), &root));
            });
        });
        group.bench_function("greek", |b| {
            b.iter(|| {
                black_box(casemapper.uppercase_to_string(black_box(&nfd), &el));
            });
        });
        group.finish();

        let mut group =
            c.benchmark_group("icu_casemap/greek_uppercasing/decomposed/upper_from_lower");
        group.bench_function("root", |b| {
            b.iter(|| {
                black_box(casemapper.uppercase_to_string(black_box(&nfd_lowercase), &root));
            });
        });
        group.bench_function("greek", |b| {
            b.iter(|| {
                black_box(casemapper.uppercase_to_string(black_box(&nfd_lowercase), &el));
            });
        });
        group.finish();
    }
}
criterion_group!(benches, overview_bench, greek_uppercasing);
criterion_main!(benches);
