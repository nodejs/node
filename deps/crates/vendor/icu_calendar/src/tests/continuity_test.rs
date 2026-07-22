// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{types::*, *};

fn check_continuity<A: AsCalendar>(mut date: Date<A>, years_to_check: usize) {
    let duration = DateDuration::for_days(1);

    let mut rata_die = date.to_rata_die();
    let mut weekday = date.day_of_week();
    let mut year = date.year();
    let mut is_in_leap_year = date.is_in_leap_year();

    for _ in 0..(366 * years_to_check) {
        let next_date = date
            .try_added_with_options(duration, Default::default())
            .unwrap();
        let next_rata_die = next_date.to_iso().to_rata_die();
        assert_eq!(next_rata_die, rata_die + 1, "{next_date:?}");
        let next_weekday = next_date.day_of_week();
        let next_year = next_date.year();
        let next_is_in_leap_year = next_date.is_in_leap_year();
        assert_eq!(
            (next_weekday as usize) % 7,
            (weekday as usize + 1) % 7,
            "{next_date:?}"
        );
        if year == next_year {
            assert_eq!(is_in_leap_year, next_is_in_leap_year, "{next_date:?}");
        }
        date = next_date;
        rata_die = next_rata_die;
        weekday = next_weekday;
        year = next_year;
        is_in_leap_year = next_is_in_leap_year;
    }
}

fn check_every_250_days<A: AsCalendar>(mut date: Date<A>, iters: usize) {
    let duration = DateDuration::for_days(250);

    let mut rata_die = date.to_rata_die();

    for _ in 0..iters {
        let next_date = date
            .try_added_with_options(duration, Default::default())
            .unwrap();
        let next_iso = next_date.to_iso();
        let next_rata_die = next_iso.to_rata_die();
        assert_eq!(next_rata_die, rata_die + 250, "{next_date:?}");
        let next_date_roundtrip = next_iso.to_calendar(Ref(next_date.calendar()));
        assert_eq!(next_date, next_date_roundtrip, "{next_date:?}");
        date = next_date;
        rata_die = next_rata_die;
    }
}

#[test]
fn test_buddhist_continuity() {
    let date = Date::try_new_buddhist(-10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_buddhist(-300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_chinese_continuity() {
    let cal = crate::cal::ChineseTraditional::new();
    let date = Date::try_new_from_codes(None, -10, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_from_codes(None, -300, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_every_250_days(date.unwrap(), 2000);
    let date = Date::try_new_from_codes(None, -10000, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_every_250_days(date.unwrap(), 2000);

    let date = Date::try_new_from_codes(None, 1899, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_continuity(date.unwrap(), 20);

    let date = Date::try_new_from_codes(None, 2099, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_continuity(date.unwrap(), 20);
}

#[test]
fn test_coptic_continuity() {
    let date = Date::try_new_coptic(-10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_coptic(-300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_korean_continuity() {
    let cal = cal::KoreanTraditional::new();
    let date = Date::try_new_from_codes(None, -10, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_from_codes(None, -300, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_every_250_days(date.unwrap(), 2000);

    let date = Date::try_new_from_codes(None, 1900, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_continuity(date.unwrap(), 20);

    let date = Date::try_new_from_codes(None, 2100, MonthCode::new_normal(1).unwrap(), 1, cal);
    check_continuity(date.unwrap(), 20);
}

#[test]
fn test_ethiopian_continuity() {
    use cal::EthiopianEraStyle::*;
    let date = Date::try_new_ethiopian(AmeteMihret, -10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_ethiopian(AmeteMihret, -300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_ethiopian_amete_alem_continuity() {
    use cal::EthiopianEraStyle::*;
    let date = Date::try_new_ethiopian(AmeteAlem, -10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_ethiopian(AmeteAlem, -300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_gregorian_continuity() {
    let date = Date::try_new_gregorian(-10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_gregorian(-300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_hebrew_continuity() {
    let date =
        Date::try_new_from_codes(None, -10, MonthCode::new_normal(1).unwrap(), 1, cal::Hebrew);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_from_codes(
        None,
        -300,
        MonthCode::new_normal(1).unwrap(),
        1,
        cal::Hebrew,
    );
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_indian_continuity() {
    let date = Date::try_new_indian(-10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_indian(-300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_hijri_civil_continuity() {
    let cal = cal::Hijri::new_tabular(
        cal::hijri::TabularAlgorithmLeapYears::TypeII,
        cal::hijri::TabularAlgorithmEpoch::Friday,
    );
    let date = Date::try_new_hijri_with_calendar(-10, 1, 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_hijri_with_calendar(-300, 1, 1, cal);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_hijri_simulated_mecca_continuity() {
    #[cfg(feature = "logging")]
    let _ = simple_logger::SimpleLogger::new().env().init();
    let cal = cal::Hijri::new_simulated_mecca();
    let date = Date::try_new_hijri_with_calendar(-10, 1, 1, cal);
    // This test is slow since it is doing astronomical calculations, so check only 3 years
    check_continuity(date.unwrap(), 3);
    let date = Date::try_new_hijri_with_calendar(-300, 1, 1, cal);
    // This test is slow since it is doing astronomical calculations, so check only 100 dates
    check_every_250_days(date.unwrap(), 100);
}

#[test]
fn test_hijri_tabular_continuity() {
    let cal = cal::Hijri::new_tabular(
        cal::hijri::TabularAlgorithmLeapYears::TypeII,
        cal::hijri::TabularAlgorithmEpoch::Thursday,
    );
    let date = Date::try_new_hijri_with_calendar(-10, 1, 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_hijri_with_calendar(-300, 1, 1, cal);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_hijri_umm_al_qura_continuity() {
    #[cfg(feature = "logging")]
    let _ = simple_logger::SimpleLogger::new().env().init();
    let cal = cal::Hijri::new_umm_al_qura();
    let date = Date::try_new_hijri_with_calendar(-10, 1, 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_hijri_with_calendar(1290, 1, 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_hijri_with_calendar(1590, 1, 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_hijri_with_calendar(-300, 1, 1, cal);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_iso_continuity() {
    let date = Date::try_new_iso(-10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_iso(-300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_japanese_continuity() {
    let cal = cal::Japanese::new();
    let cal = Ref(&cal);
    let date = Date::try_new_japanese_with_calendar("heisei", 20, 1, 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_japanese_with_calendar("bce", 500, 1, 1, cal);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_japanese_extended_continuity() {
    let cal = cal::JapaneseExtended::new();
    let cal = Ref(&cal);
    let date = Date::try_new_japanese_extended_with_calendar("heisei", 20, 1, 1, cal);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_japanese_extended_with_calendar("bce", 500, 1, 1, cal);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_persian_continuity() {
    let date = Date::try_new_persian(-10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_persian(-300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}

#[test]
fn test_roc_continuity() {
    let date = Date::try_new_roc(-10, 1, 1);
    check_continuity(date.unwrap(), 20);
    let date = Date::try_new_roc(-300, 1, 1);
    check_every_250_days(date.unwrap(), 2000);
}
