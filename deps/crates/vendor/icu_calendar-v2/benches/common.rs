// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[macro_export]
macro_rules! bench_all_calendars {
    ($group:ident, $bench_one_calendar:ident $($args:tt)*) => {
        $bench_one_calendar(&mut $group, "calendar/iso", icu::calendar::cal::Iso $($args)*);

        $bench_one_calendar(
            &mut $group,
            "calendar/buddhist",
            icu::calendar::cal::Buddhist
            $($args)*
        );

        $bench_one_calendar(&mut $group, "calendar/coptic", icu::calendar::cal::Coptic $($args)*);

        $bench_one_calendar(
            &mut $group,
            "calendar/ethiopic",
            icu::calendar::cal::Ethiopian::new()
            $($args)*
        );

        $bench_one_calendar(&mut $group, "calendar/indian", icu::calendar::cal::Indian $($args)*);

        $bench_one_calendar(&mut $group, "calendar/julian", icu::calendar::cal::Julian $($args)*);

        $bench_one_calendar(
            &mut $group,
            "calendar/chinese_cached",
            icu::calendar::cal::ChineseTraditional::new()
            $($args)*
        );

        $bench_one_calendar(
            &mut $group,
            "calendar/gregorian",
            icu::calendar::cal::Gregorian
            $($args)*
        );

        $bench_one_calendar(&mut $group, "calendar/hebrew", icu::calendar::cal::Hebrew $($args)*);

        #[allow(deprecated)]
        $bench_one_calendar(
            &mut $group,
            "calendar/islamic/observational",
            icu::calendar::cal::Hijri::new_simulated_mecca()
            $($args)*
        );

        $bench_one_calendar(
            &mut $group,
            "calendar/islamic/civil",
            icu::calendar::cal::Hijri::new_tabular(
                icu::calendar::cal::hijri::TabularAlgorithmLeapYears::TypeII,
                icu::calendar::cal::hijri::TabularAlgorithmEpoch::Friday,
            )
            $($args)*
        );

        $bench_one_calendar(
            &mut $group,
            "calendar/islamic/ummalqura",
            icu::calendar::cal::Hijri::new_umm_al_qura()
            $($args)*
        );

        $bench_one_calendar(
            &mut $group,
            "calendar/islamic/tabular",
            icu::calendar::cal::Hijri::new_tabular(
                icu::calendar::cal::hijri::TabularAlgorithmLeapYears::TypeII,
                icu::calendar::cal::hijri::TabularAlgorithmEpoch::Thursday,
            )
            $($args)*
        );
    };
}
