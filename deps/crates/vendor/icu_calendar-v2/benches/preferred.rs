// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{Criterion, black_box, criterion_group, criterion_main};
use icu::calendar::AnyCalendarKind;
use icu::calendar::preferences::CalendarPreferences;
use icu::locale::locale;

/// Loading the preferred calendar is an operation often in the critical path
/// for datetime formatting. We want to keep it from regressing. See #8375.
fn preferred_benches(c: &mut Criterion) {
    let prefs = &[
        CalendarPreferences::from(locale!("en")),
        CalendarPreferences::from(locale!("fa")),
        CalendarPreferences::from(locale!("ar-EG")),
        CalendarPreferences::from(locale!("ar-SA-u-ca-islamic")),
        CalendarPreferences::from(locale!("he-u-ca-hebrew")),
    ];
    let mut group = c.benchmark_group("preferred");
    group.bench_function("try_new", |b| {
        b.iter(|| black_box(&prefs).map(AnyCalendarKind::try_new))
    });
    group.finish();
}

criterion_group!(benches, preferred_benches);
criterion_main!(benches);
