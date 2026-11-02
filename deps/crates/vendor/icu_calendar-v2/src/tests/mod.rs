// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod arithmetic;
mod constructor_roundtrip;
mod continuity_test;
mod date_arithmetic_snapshot;
mod exhaustive;
mod extended_year;
mod extrema;
mod month;
mod not_enough_fields;

use crate::Date;
use crate::types::Month;
use calendrical_calculations::rata_die::RataDie;

macro_rules! test_all_cals {
    ($(#[$meta:meta])* fn $name:ident<C: Calendar + Copy>($cal:ident: C) $tt:tt) => {
        mod $name {
            #[allow(unused_imports)]
            use super::*;

            fn test<C: crate::Calendar + Copy>(cal: C) where Date<C>: Ord {
                let $cal = cal;
                $tt
            }

            $(#[$meta])*
            #[test]
            fn buddhist() {
                test(crate::cal::Buddhist);
            }

            $(#[$meta])*
            #[test]
            fn chinese_traditional() {
                test(crate::cal::east_asian_traditional::EastAsianTraditional(crate::cal::east_asian_traditional_internal::EastAsianTraditionalYears::china()));
            }

            $(#[$meta])*
            #[test]
            fn coptic() {
                test(crate::cal::Coptic);
            }

            $(#[$meta])*
            #[test]
            fn korean_traditional() {
                test(crate::cal::east_asian_traditional::EastAsianTraditional(crate::cal::east_asian_traditional_internal::EastAsianTraditionalYears::korea()));
            }

            $(#[$meta])*
            #[test]
            fn ethiopian() {
                test(crate::cal::Ethiopian::new());
            }

            $(#[$meta])*
            #[test]
            fn ethiopian_amete_alem() {
                test(crate::cal::Ethiopian::new_with_era_style(crate::cal::EthiopianEraStyle::AmeteAlem));
            }

            $(#[$meta])*
            #[test]
            fn gregorian() {
                test(crate::cal::Gregorian);
            }

            $(#[$meta])*
            #[test]
            fn hebrew() {
                test(crate::cal::Hebrew::new());
            }

            $(#[$meta])*
            #[test]
            fn hijri_tabular_friday() {
                test(crate::cal::Hijri::new_tabular(crate::cal::hijri::TabularAlgorithmLeapYears::TypeII, crate::cal::hijri::TabularAlgorithmEpoch::Friday));
            }

            $(#[$meta])*
            #[test]
            fn hijri_tabular_thursday() {
                test(crate::cal::Hijri::new_tabular(crate::cal::hijri::TabularAlgorithmLeapYears::TypeII, crate::cal::hijri::TabularAlgorithmEpoch::Thursday));
            }

            $(#[$meta])*
            #[test]
            fn hijri_uaq() {
                test(crate::cal::Hijri::new_umm_al_qura());
            }

            $(#[$meta])*
            #[test]
            fn indian() {
                test(crate::cal::Indian::new());
            }

            $(#[$meta])*
            #[test]
            fn iso() {
                test(crate::cal::Iso::new());
            }

            $(#[$meta])*
            #[test]
            fn julian() {
                test(crate::cal::Julian::new());
            }

            $(#[$meta])*
            #[test]
            fn japanese() {
                test(crate::cal::Japanese::new());
            }

            $(#[$meta])*
            #[test]
            fn persian() {
                test(crate::cal::Persian::new());
            }

            $(#[$meta])*
            #[test]
            fn roc() {
                test(crate::cal::Roc);
            }
        }
    };
}
use test_all_cals;

pub(crate) fn get_interesting_rds() -> Vec<RataDie> {
    let mut rds = Vec::new();

    // Iso / Gregorian leap day
    let date = Date::try_new_iso(2020, 2, 29).expect("2020-02-29 is a valid ISO date");
    rds.push(date.to_rata_die());

    // Hebrew leap month (Adar I) in leap year 5784
    let date = Date::try_new_hebrew_v2(5784, Month::leap(5), 15).expect("Valid Hebrew date");
    rds.push(date.to_rata_die());

    // Hebrew Adar II in leap year 5784
    let date = Date::try_new_hebrew_v2(5784, Month::new(6), 15).expect("Valid Hebrew date");
    rds.push(date.to_rata_die());

    // Hebrew non-Adar month in leap year 5784
    let date = Date::try_new_hebrew_v2(5784, Month::new(1), 15).expect("Valid Hebrew date");
    rds.push(date.to_rata_die());

    // Chinese leap month date
    let date =
        Date::try_new_chinese_traditional(2023, Month::leap(2), 1).expect("Valid Chinese date");
    rds.push(date.to_rata_die());

    // Chinese leap year 2023, non-leap month
    let date =
        Date::try_new_chinese_traditional(2023, Month::new(1), 1).expect("Valid Chinese date");
    rds.push(date.to_rata_die());

    // Japanese era transition
    let date = Date::try_new_japanese_with_calendar("reiwa", 1, 5, 1, crate::cal::Japanese::new())
        .expect("Valid Japanese date");
    rds.push(date.to_rata_die());

    // Ethiopic month 13
    let date = Date::try_new_ethiopian(crate::cal::EthiopianEraStyle::AmeteMihret, 2015, 13, 5)
        .expect("Valid Ethiopian date");
    rds.push(date.to_rata_die());

    // Coptic month 13
    let date = Date::try_new_coptic(1686, 13, 5).expect("Valid Coptic date");
    rds.push(date.to_rata_die());

    // Add some more general dates
    rds.push(RataDie::new(0));
    rds.push(RataDie::new(1));
    rds.push(RataDie::new(-1));
    rds.push(RataDie::new(730000)); // ~2000 CE
    rds.push(RataDie::new(100));
    rds.push(RataDie::new(-100));
    rds.push(RataDie::new(1000000)); // ~2739 CE
    rds.push(RataDie::new(-1000000)); // ~ -2739 CE
    rds.push(RataDie::new(123456));
    rds.push(RataDie::new(-123456));

    rds
}
