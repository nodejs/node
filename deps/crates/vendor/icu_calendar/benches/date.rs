// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DateFixture(pub Vec<Test>);

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Test {
    pub year: i32,
    pub month: u8,
    pub day: u8,
    pub hour: u8,
    pub minute: u8,
    pub second: u8,
}

use criterion::{
    black_box, criterion_group, criterion_main, measurement::WallTime, BenchmarkGroup, Criterion,
};
use icu_calendar::{
    options::{DateAddOptions, Overflow},
    types, AsCalendar, Calendar, Date,
};

fn bench_date<A: AsCalendar>(date: &mut Date<A>) {
    // black_box used to avoid compiler optimization.
    // Arithmetic
    let mut options = DateAddOptions::default();
    options.overflow = Some(Overflow::Constrain);
    date.try_add_with_options(
        types::DateDuration {
            is_negative: false,
            years: black_box(1),
            months: black_box(2),
            weeks: black_box(3),
            days: black_box(4),
        },
        options,
    )
    .unwrap();

    // Retrieving vals
    let _ = black_box(date.year());
    let _ = black_box(date.month());
    let _ = black_box(date.day_of_month());

    // Conversion to ISO.
    let _ = black_box(date.to_iso());
}

fn bench_calendar<C: Clone + Calendar>(
    group: &mut BenchmarkGroup<WallTime>,
    name: &str,
    fxs: &DateFixture,
    calendar: C,
    calendar_date_init: impl Fn(i32, u8, u8, C) -> Date<C>,
) {
    group.bench_function(name, |b| {
        b.iter(|| {
            for fx in &fxs.0 {
                // Instantion from int
                let mut instantiated_date_calendar =
                    calendar_date_init(fx.year, fx.month, fx.day, calendar.clone());

                // Conversion from ISO
                let date_iso = Date::try_new_iso(fx.year, fx.month, fx.day).unwrap();
                let mut converted_date_calendar = Date::new_from_iso(date_iso, calendar.clone());

                bench_date(&mut instantiated_date_calendar);
                bench_date(&mut converted_date_calendar);
            }
        })
    });
}

fn date_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("date");
    let fxs = serde_json::from_str::<DateFixture>(include_str!("fixtures/datetimes.json")).unwrap();

    bench_calendar(
        &mut group,
        "calendar/overview",
        &fxs,
        icu::calendar::cal::Iso,
        |y, m, d, _| Date::try_new_iso(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/buddhist",
        &fxs,
        icu::calendar::cal::Buddhist,
        |y, m, d, _| Date::try_new_buddhist(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/coptic",
        &fxs,
        icu::calendar::cal::Coptic,
        |y, m, d, _| Date::try_new_coptic(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/ethiopic",
        &fxs,
        icu::calendar::cal::Ethiopian::new(),
        |y, m, d, _| {
            Date::try_new_ethiopian(icu::calendar::cal::EthiopianEraStyle::AmeteMihret, y, m, d)
                .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/indian",
        &fxs,
        icu::calendar::cal::Indian,
        |y, m, d, _| Date::try_new_indian(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/persian",
        &fxs,
        icu::calendar::cal::Persian,
        |y, m, d, _| Date::try_new_persian(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/roc",
        &fxs,
        icu::calendar::cal::Roc,
        |y, m, d, _| Date::try_new_roc(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/julian",
        &fxs,
        icu::calendar::cal::Julian,
        |y, m, d, _| Date::try_new_julian(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/chinese_cached",
        &fxs,
        icu::calendar::cal::ChineseTraditional::new(),
        |y, m, d, c| {
            Date::try_new_from_codes(None, y, types::MonthCode::new_normal(m).unwrap(), d, c)
                .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/dangi_cached",
        &fxs,
        icu::calendar::cal::KoreanTraditional::new(),
        |y, m, d, c| {
            Date::try_new_from_codes(None, y, types::MonthCode::new_normal(m).unwrap(), d, c)
                .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/hebrew",
        &fxs,
        icu::calendar::cal::Hebrew,
        |y, m, d, c| {
            Date::try_new_from_codes(None, y, types::MonthCode::new_normal(m).unwrap(), d, c)
                .unwrap()
        },
    );

    bench_calendar(
        &mut group,
        "calendar/gregorian",
        &fxs,
        icu::calendar::cal::Gregorian,
        |y, m, d, _| Date::try_new_gregorian(y, m, d).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/civil",
        &fxs,
        icu::calendar::cal::Hijri::new_tabular(
            icu::calendar::cal::hijri::TabularAlgorithmLeapYears::TypeII,
            icu::calendar::cal::hijri::TabularAlgorithmEpoch::Friday,
        ),
        |y, m, d, c| Date::try_new_hijri_with_calendar(y, m, d, c).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/tabular",
        &fxs,
        icu::calendar::cal::Hijri::new_tabular(
            icu::calendar::cal::hijri::TabularAlgorithmLeapYears::TypeII,
            icu::calendar::cal::hijri::TabularAlgorithmEpoch::Thursday,
        ),
        |y, m, d, c| Date::try_new_hijri_with_calendar(y, m, d, c).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/ummalqura",
        &fxs,
        icu::calendar::cal::Hijri::new_umm_al_qura(),
        |y, m, d, c| Date::try_new_hijri_with_calendar(y, m, d, c).unwrap(),
    );

    bench_calendar(
        &mut group,
        "calendar/islamic/observational",
        &fxs,
        icu::calendar::cal::Hijri::new_simulated_mecca(),
        |y, m, d, c| Date::try_new_hijri_with_calendar(y, m, d, c).unwrap(),
    );

    group.finish();
}

criterion_group!(benches, date_benches);
criterion_main!(benches);
