// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::cal::hijri::{
    HijriYear, Rules, TabularAlgorithm, TabularAlgorithmEpoch, TabularAlgorithmLeapYears,
};
use icu_calendar::cal::Hijri;
use icu_calendar::Date;

/// [`Hijri`] [`Rules`] based on an astronomical simulation for Mecca.
///
/// These simulations use methods published by E. M. Reingold, S. K. Shaukat, et al.[^1]
/// which are not officially recognized in any region and do not match sightings
/// on the ground.
///
/// [^1]: See [`calendrical_calculations::islamic::observational_islamic_from_fixed`]
#[derive(Debug, Clone, Copy)]
struct ReingoldSimulation;

impl icu_calendar::cal::scaffold::UnstableSealed for ReingoldSimulation {}
impl Rules for ReingoldSimulation {
    fn year(&self, extended_year: i32) -> HijriYear {
        if !(1317..1567).contains(&extended_year) {
            // Reingold only works for current years
            return TabularAlgorithm::new(
                TabularAlgorithmLeapYears::TypeII,
                TabularAlgorithmEpoch::Friday,
            )
            .year(extended_year);
        }

        HijriYear::try_new(
            extended_year,
            core::array::from_fn::<_, 13, _>(|m| {
                calendrical_calculations::islamic::fixed_from_observational_islamic(
                    extended_year + m as i32 / 12,
                    m as u8 % 12 + 1,
                    1,
                    calendrical_calculations::islamic::MECCA,
                )
            }),
        )
        .unwrap()
    }

    type DateCompatibilityError = core::convert::Infallible;

    fn check_date_compatibility(&self, &Self: &Self) -> Result<(), Self::DateCompatibilityError> {
        Ok(())
    }

    fn debug_name(&self) -> &'static str {
        "Hijri (Reingold, Mecca)"
    }
}

#[test]
fn main() {
    Date::try_new_hijri_with_calendar(1390, 1, 30, Hijri(ReingoldSimulation)).unwrap();

    Date::try_new_iso(2000, 5, 5)
        .unwrap()
        .to_calendar(Hijri(ReingoldSimulation));
}
