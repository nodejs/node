// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::cal::*;
use crate::Calendar;
use crate::Date;
use crate::Ref;

#[track_caller]
fn check_extrema<C: Calendar>(cal: C) {
    // Minimum and maximum dates allowed in ECMA-262 Temporal.
    let min_date_iso = Date::try_new_iso(-271821, 4, 19).unwrap();
    let max_date_iso = Date::try_new_iso(275760, 9, 13).unwrap();
    let min_date = min_date_iso.to_calendar(Ref(&cal));
    let max_date = max_date_iso.to_calendar(Ref(&cal));

    println!(
        "min.year = {:?}, max.year = {:?} (cal = {})",
        min_date.year(),
        max_date.year(),
        cal.debug_name()
    );
}

// Test all calendars that have any amount of tricky mathematics
// to ensure that they do not trigger debug assertions for large dates.

#[test]
fn check_extrema_chinese() {
    check_extrema(ChineseTraditional::new())
}

#[test]
fn check_extrema_korean() {
    check_extrema(KoreanTraditional::new())
}

#[test]
fn check_extrema_hijri_simulated_mecca() {
    check_extrema(Hijri::new_simulated_mecca())
}

#[test]
fn check_extrema_hijri_uaq() {
    check_extrema(Hijri::new_umm_al_qura())
}

#[test]
fn check_extrema_hijri_tabular() {
    check_extrema(Hijri::new_tabular(
        hijri::TabularAlgorithmLeapYears::TypeII,
        hijri::TabularAlgorithmEpoch::Thursday,
    ));
    check_extrema(Hijri::new_tabular(
        hijri::TabularAlgorithmLeapYears::TypeII,
        hijri::TabularAlgorithmEpoch::Friday,
    ));
}

#[test]
fn check_extrema_hebrew() {
    check_extrema(Hebrew::new())
}

#[test]
fn check_extrema_persian() {
    check_extrema(Persian::new())
}
