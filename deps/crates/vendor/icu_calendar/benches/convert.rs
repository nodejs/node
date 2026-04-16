// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{
    black_box, criterion_group, criterion_main, measurement::WallTime, BenchmarkGroup, Criterion,
};
use icu_calendar::{Calendar, Date};

#[macro_use]
mod common;

fn bench_calendar<C: Copy + Calendar>(
    group: &mut BenchmarkGroup<WallTime>,
    name: &str,
    calendar: C,
) {
    let iso = Date::try_new_iso(2023, 8, 16).unwrap();
    group.bench_function(name, |b| {
        b.iter(|| {
            let converted = black_box(iso).to_calendar(calendar);
            let year = black_box(converted.year());
            let month = black_box(converted.month());
            let day = black_box(converted.day_of_month());
            black_box((converted, year, month, day))
        })
    });
}

fn convert_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("convert");

    bench_all_calendars!(group, bench_calendar);

    group.finish();
}

criterion_group!(benches, convert_benches);
criterion_main!(benches);
