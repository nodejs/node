// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod arithmetic;
mod continuity_test;
mod date_arithmetic_snapshot;
mod exhaustive;
mod extended_year;
mod extrema;
mod month;
mod not_enough_fields;

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
