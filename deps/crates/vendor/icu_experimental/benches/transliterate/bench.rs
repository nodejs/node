// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{black_box, BenchmarkId, Criterion};
use criterion::{criterion_group, criterion_main};
use icu_locale_core::Locale;

use icu_experimental::transliterate::Transliterator;

struct BenchDataContent {
    pub num: usize,
    pub name: String,
    pub translit: Transliterator,
    pub input: String,
}

fn owned_inputs(content: &str) -> Vec<String> {
    content
        .lines()
        .filter(|&s| !s.starts_with('#'))
        .map(|s| s.split('\t').next().unwrap().to_string())
        .collect::<Vec<String>>()
}

fn bench_data_from_sources(locale_str: &str, source: &str) -> Vec<BenchDataContent> {
    let locale: Locale = locale_str.parse().unwrap();
    let inputs = owned_inputs(source);
    inputs
        .into_iter()
        .enumerate()
        .map(|(idx, input)| BenchDataContent {
            num: idx + 1,
            name: locale_str.to_string(),
            translit: Transliterator::try_new(&locale).unwrap(),
            input,
        })
        .collect()
}

// TODO(#3957): Better bench inputs
fn bench_data() -> Vec<BenchDataContent> {
    let mut bench_data = Vec::new();
    bench_data.extend(bench_data_from_sources(
        "und-Arab-t-und-beng",
        include_str!("../../tests/transliterate/data/fixtures/und-Arab-t-und-beng.txt"),
    ));

    bench_data.into_iter().take(5).collect()
}

fn function_under_bench(translit: &Transliterator, input: &str) {
    translit.transliterate(input.to_string());
}

pub fn criterion_benchmark(criterion: &mut Criterion) {
    let group_name = "transliterate";

    let mut group = criterion.benchmark_group(group_name);

    for bench_data_content in black_box(bench_data()) {
        group.bench_function(
            BenchmarkId::from_parameter(format!(
                "transliterate_{}_{}",
                bench_data_content.name, bench_data_content.num
            )),
            |bencher| {
                bencher.iter(|| {
                    function_under_bench(&bench_data_content.translit, &bench_data_content.input)
                })
            },
        );
    }
    group.finish();
}

criterion_group!(benches, criterion_benchmark,);

criterion_main!(benches);
