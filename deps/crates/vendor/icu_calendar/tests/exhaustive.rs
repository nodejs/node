// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::*;

const MAGNITUDE: i32 = 1_000_000;

// Check rd -> date -> iso -> date -> rd for whole range
#[test]
#[ignore] // takes about 200 seconds in release-with-assertions
fn check_round_trip() {
    fn test<C: Calendar>(cal: C) {
        let cal = Ref(&cal);
        let low = Date::try_new_iso(-MAGNITUDE, 1, 1).unwrap().to_rata_die();
        let high = Date::try_new_iso(MAGNITUDE, 12, 31).unwrap().to_rata_die();
        let mut prev = Date::from_rata_die(low, cal);
        let mut curr = low + 1;
        while curr <= high {
            let date = Date::from_rata_die(curr, cal);
            assert!(prev < date);

            let rd = date.to_rata_die();
            assert_eq!(rd, curr, "{}", cal.as_calendar().debug_name());

            let date2 = Date::new_from_iso(date.to_iso(), cal);
            assert_eq!(date, date2, "{:?}", cal.as_calendar().debug_name());

            prev = date;
            curr += 1;
        }
    }

    for_each_calendar!(test);
}

#[test]
#[ignore] // takes about 90 seconds in release-with-assertions
fn check_from_fields() {
    fn test<C: Calendar>(cal: C) {
        let cal = Ref(&cal);

        let codes = (1..19)
            .flat_map(|i| {
                [
                    types::MonthCode::new_normal(i).unwrap(),
                    types::MonthCode::new_leap(i).unwrap(),
                ]
                .into_iter()
            })
            .collect::<Vec<_>>();
        for year in -MAGNITUDE..=MAGNITUDE {
            if year % 50000 == 0 {
                println!("{} {year:?}", cal.as_calendar().debug_name());
            }
            for overflow in [options::Overflow::Constrain, options::Overflow::Reject] {
                let mut options = options::DateFromFieldsOptions::default();
                options.overflow = Some(overflow);
                for mut fields in codes
                    .iter()
                    .map(|m| {
                        let mut fields = types::DateFields::default();
                        fields.month_code = Some(m.0.as_bytes());
                        fields
                    })
                    .chain((1..20).map(|m| {
                        let mut fields = types::DateFields::default();
                        fields.ordinal_month = Some(m);
                        fields
                    }))
                {
                    for day in 1..50 {
                        fields.extended_year = Some(year);
                        fields.day = Some(day);
                        let _ = Date::try_from_fields(fields, options, Ref(&cal));
                    }
                }
            }
        }
    }
    for_each_calendar!(test);
}

macro_rules! for_each_calendar {
    ($f:ident) => {
        [
            || {
                $f(cal::Buddhist);
                println!("Buddhist done");
            },
            || {
                $f(cal::east_asian_traditional::EastAsianTraditional(
                    EastAsianTraditionalYears::new(cal::east_asian_traditional::China::default()),
                ));
                println!("Chinese done");
            },
            || {
                $f(cal::Coptic);
                println!("Coptic done");
            },
            || {
                $f(cal::east_asian_traditional::EastAsianTraditional(
                    EastAsianTraditionalYears::new(cal::east_asian_traditional::Korea::default()),
                ));
                println!("Korean done");
            },
            || {
                $f(cal::Ethiopian::new());
                println!("Ethiopian done");
            },
            || {
                $f(cal::Ethiopian::new_with_era_style(
                    cal::EthiopianEraStyle::AmeteAlem,
                ));
                println!("Ethiopian (Amete Alem) done");
            },
            || {
                $f(cal::Gregorian);
                println!("Gregorian done");
            },
            || {
                $f(cal::Hebrew::new());
                println!("Hebrew done");
            },
            || {
                $f(cal::Hijri::new_tabular(
                    cal::hijri::TabularAlgorithmLeapYears::TypeII,
                    cal::hijri::TabularAlgorithmEpoch::Friday,
                ));
                println!("Hijri (tabular) done");
            },
            || {
                $f(cal::Hijri::new_tabular(
                    cal::hijri::TabularAlgorithmLeapYears::TypeII,
                    cal::hijri::TabularAlgorithmEpoch::Thursday,
                ));
                println!("Hijri (astronomical) done");
            },
            || {
                $f(cal::Hijri::new_umm_al_qura());
                println!("Hijri (UAQ) done");
            },
            || {
                $f(cal::Indian::new());
                println!("Indian done");
            },
            || {
                $f(cal::Iso::new());
                println!("Iso done");
            },
            || {
                $f(cal::Julian::new());
                println!("Julian done");
            },
            || {
                $f(cal::Japanese::new());
                println!("Japanese done");
            },
            || {
                $f(cal::JapaneseExtended::new());
                println!("JapaneseExtended done");
            },
            || {
                $f(cal::Persian::new());
                println!("Persian done");
            },
            || {
                $f(cal::Roc);
                println!("Roc done");
            },
        ]
        .map(std::thread::spawn)
        .into_iter()
        .for_each(|h| h.join().unwrap());
    };
}
use for_each_calendar;

// Precalculates Chinese years, significant performance improvement
#[derive(Debug, Clone)]
struct EastAsianTraditionalYears(Vec<cal::east_asian_traditional::EastAsianTraditionalYearData>);

impl EastAsianTraditionalYears {
    fn new<R: cal::east_asian_traditional::Rules>(r: R) -> Self {
        Self(
            ((-MAGNITUDE - 1)..=MAGNITUDE)
                .map(|i| r.year_data(i))
                .collect(),
        )
    }
}

impl cal::scaffold::UnstableSealed for EastAsianTraditionalYears {}
impl cal::east_asian_traditional::Rules for EastAsianTraditionalYears {
    fn year_data(
        &self,
        related_iso: i32,
    ) -> cal::east_asian_traditional::EastAsianTraditionalYearData {
        self.0[(related_iso + MAGNITUDE + 1) as usize]
    }
}
