// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use criterion::{
    black_box, criterion_group, criterion_main, measurement::WallTime, BenchmarkGroup, Criterion,
};
use icu_calendar::{Calendar, Date, Ref};

fn bench_calendar<C: Clone + Calendar>(
    group: &mut BenchmarkGroup<WallTime>,
    name: &str,
    calendar: C,
) {
    let iso = Date::try_new_iso(2023, 8, 16).unwrap();
    group.bench_function(name, |b| {
        b.iter(|| {
            let converted = black_box(iso).to_calendar(Ref(&calendar));
            let year = black_box(converted.year());
            let month = black_box(converted.month());
            let day = black_box(converted.day_of_month());
            black_box((converted, year, month, day))
        })
    });
}

fn convert_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("convert");

    bench_calendar(&mut group, "calendar/iso", icu::calendar::cal::Iso);

    bench_calendar(
        &mut group,
        "calendar/buddhist",
        icu::calendar::cal::Buddhist,
    );

    bench_calendar(&mut group, "calendar/coptic", icu::calendar::cal::Coptic);

    bench_calendar(
        &mut group,
        "calendar/ethiopic",
        icu::calendar::cal::Ethiopian::new(),
    );

    bench_calendar(&mut group, "calendar/indian", icu::calendar::cal::Indian);

    bench_calendar(&mut group, "calendar/julian", icu::calendar::cal::Julian);

    bench_calendar(
        &mut group,
        "calendar/chinese_calculating",
        icu::calendar::cal::Chinese::new_always_calculating(),
    );

    bench_calendar(
        &mut group,
        "calendar/chinese_cached",
        icu::calendar::cal::Chinese::new(),
    );

    bench_calendar(
        &mut group,
        "calendar/gregorian",
        icu::calendar::cal::Gregorian,
    );

    bench_calendar(&mut group, "calendar/hebrew", icu::calendar::cal::Hebrew);

    bench_calendar(
        &mut group,
        "calendar/islamic/observational",
        icu::calendar::cal::HijriSimulated::new_mecca_always_calculating(),
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/civil",
        icu::calendar::cal::HijriTabular::new(
            icu::calendar::cal::HijriTabularLeapYears::TypeII,
            icu::calendar::cal::HijriTabularEpoch::Friday,
        ),
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/ummalqura",
        icu::calendar::cal::HijriUmmAlQura::new(),
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/tabular",
        icu::calendar::cal::HijriTabular::new(
            icu::calendar::cal::HijriTabularLeapYears::TypeII,
            icu::calendar::cal::HijriTabularEpoch::Thursday,
        ),
    );

    group.finish();
}

criterion_group!(benches, convert_benches);
criterion_main!(benches);
