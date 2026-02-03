// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::*;
use core::marker::PhantomData;

fn check_continuity<A: AsCalendar>(mut date: Date<A>) {
    let one_day_duration = DateDuration::<A::Calendar> {
        years: 0,
        months: 0,
        weeks: 0,
        days: 1,
        marker: PhantomData,
    };

    let mut rata_die = date.to_rata_die();
    let mut weekday = date.day_of_week();
    let mut year = date.year();
    let mut is_in_leap_year = date.is_in_leap_year();

    for _ in 0..(366 * 20) {
        let next_date = date.added(one_day_duration);
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

fn check_every_250_days<A: AsCalendar>(mut date: Date<A>) {
    let one_thousand_days_duration = DateDuration::<A::Calendar> {
        years: 0,
        months: 0,
        weeks: 0,
        days: 250,
        marker: PhantomData,
    };

    let mut rata_die = date.to_rata_die();

    for _ in 0..2000 {
        let next_date = date.added(one_thousand_days_duration);
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
    check_continuity(date.unwrap());
    let date = Date::try_new_buddhist(-300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_chinese_continuity() {
    let cal = crate::cal::Chinese::new();
    let cal = Ref(&cal);
    let date = Date::try_new_chinese_with_calendar(-10, 1, 1, cal);
    check_continuity(date.unwrap());
    let date = Date::try_new_chinese_with_calendar(-300, 1, 1, cal);
    check_every_250_days(date.unwrap());
    let date = Date::try_new_chinese_with_calendar(-10000, 1, 1, cal);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_coptic_continuity() {
    let date = Date::try_new_coptic(-10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_coptic(-300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_dangi_continuity() {
    let cal = crate::cal::Dangi::new();
    let cal = Ref(&cal);
    let date = Date::try_new_dangi_with_calendar(-10, 1, 1, cal);
    check_continuity(date.unwrap());
    let date = Date::try_new_dangi_with_calendar(-300, 1, 1, cal);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_ethiopian_continuity() {
    use crate::cal::EthiopianEraStyle::*;
    let date = Date::try_new_ethiopian(AmeteMihret, -10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_ethiopian(AmeteMihret, -300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_ethiopian_amete_alem_continuity() {
    use crate::cal::EthiopianEraStyle::*;
    let date = Date::try_new_ethiopian(AmeteAlem, -10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_ethiopian(AmeteAlem, -300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_gregorian_continuity() {
    let date = Date::try_new_gregorian(-10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_gregorian(-300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_hebrew_continuity() {
    let date = Date::try_new_hebrew(-10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_hebrew(-300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_indian_continuity() {
    let date = Date::try_new_indian(-10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_indian(-300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_hijri_civil_continuity() {
    let cal = crate::cal::HijriTabular::new(
        crate::cal::HijriTabularLeapYears::TypeII,
        crate::cal::HijriTabularEpoch::Friday,
    );
    let cal = Ref(&cal);
    let date = Date::try_new_hijri_tabular_with_calendar(-10, 1, 1, cal);
    check_continuity(date.unwrap());
    let date = Date::try_new_hijri_tabular_with_calendar(-300, 1, 1, cal);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_hijri_simulated_mecca_continuity() {
    #[cfg(feature = "logging")]
    let _ = simple_logger::SimpleLogger::new().env().init();
    let cal = crate::cal::HijriSimulated::new_mecca_always_calculating();
    let cal = Ref(&cal);
    let date = Date::try_new_simulated_hijri_with_calendar(-10, 1, 1, cal);
    check_continuity(date.unwrap());
    let date = Date::try_new_simulated_hijri_with_calendar(-300, 1, 1, cal);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_hijri_tabular_continuity() {
    let cal = crate::cal::HijriTabular::new(
        crate::cal::HijriTabularLeapYears::TypeII,
        crate::cal::HijriTabularEpoch::Thursday,
    );
    let cal = Ref(&cal);
    let date = Date::try_new_hijri_tabular_with_calendar(-10, 1, 1, cal);
    check_continuity(date.unwrap());
    let date = Date::try_new_hijri_tabular_with_calendar(-300, 1, 1, cal);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_hijri_umm_al_qura_continuity() {
    #[cfg(feature = "logging")]
    let _ = simple_logger::SimpleLogger::new().env().init();
    let date = Date::try_new_ummalqura(-10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_ummalqura(1290, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_ummalqura(1590, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_ummalqura(-300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_iso_continuity() {
    let date = Date::try_new_iso(-10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_iso(-300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_japanese_continuity() {
    let cal = crate::cal::Japanese::new();
    let cal = Ref(&cal);
    let date = Date::try_new_japanese_with_calendar("heisei", 20, 1, 1, cal);
    check_continuity(date.unwrap());
    let date = Date::try_new_japanese_with_calendar("bce", 500, 1, 1, cal);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_japanese_extended_continuity() {
    let cal = crate::cal::JapaneseExtended::new();
    let cal = Ref(&cal);
    let date = Date::try_new_japanese_extended_with_calendar("heisei", 20, 1, 1, cal);
    check_continuity(date.unwrap());
    let date = Date::try_new_japanese_extended_with_calendar("bce", 500, 1, 1, cal);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_persian_continuity() {
    let date = Date::try_new_persian(-10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_persian(-300, 1, 1);
    check_every_250_days(date.unwrap());
}

#[test]
fn test_roc_continuity() {
    let date = Date::try_new_roc(-10, 1, 1);
    check_continuity(date.unwrap());
    let date = Date::try_new_roc(-300, 1, 1);
    check_every_250_days(date.unwrap());
}
