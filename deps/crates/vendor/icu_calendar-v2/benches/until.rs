// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{
    black_box, criterion_group, criterion_main, measurement::WallTime, BenchmarkGroup, Criterion,
};
use icu_calendar::options::{DateDifferenceOptions, DateDurationUnit};
use icu_calendar::types::RataDie;
use icu_calendar::{Calendar, Date};

#[macro_use]
mod common;

fn bench_calendar<C: Copy + Calendar>(
    group: &mut BenchmarkGroup<WallTime>,
    name: &str,
    calendar: C,
    unit: DateDurationUnit,
) {
    // 300000 isn't a huge range, but these benchmarks are too slow otherwise.
    //
    // We can add more benchmarks once things are somewhat optimized
    let very_far_past = Date::from_rata_die(RataDie::new(-300_000_000), calendar);
    let far_past = Date::from_rata_die(RataDie::new(-300_000), calendar);
    let now = Date::from_rata_die(RataDie::new(0), calendar);
    let mut options = DateDifferenceOptions::default();
    options.largest_unit = Some(unit);
    group.bench_function(format!("{name}/far_past"), |b| {
        b.iter(|| black_box(far_past.try_until_with_options(&now, options).unwrap()))
    });
    // Still slow :/
    if unit != DateDurationUnit::Months {
        group.bench_function(format!("{name}/very_far_past"), |b| {
            b.iter(|| black_box(very_far_past.try_until_with_options(&now, options).unwrap()))
        });
    }
}

fn until_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("until/years");
    bench_all_calendars!(group, bench_calendar, DateDurationUnit::Years);
    group.finish();

    let mut group = c.benchmark_group("until/months");
    bench_all_calendars!(group, bench_calendar, DateDurationUnit::Months);
    group.finish();

    let mut group = c.benchmark_group("until/weeks");
    bench_all_calendars!(group, bench_calendar, DateDurationUnit::Weeks);
    group.finish();

    let mut group = c.benchmark_group("until/days");
    bench_all_calendars!(group, bench_calendar, DateDurationUnit::Days);
    group.finish();
}

criterion_group!(benches, until_benches);
criterion_main!(benches);
