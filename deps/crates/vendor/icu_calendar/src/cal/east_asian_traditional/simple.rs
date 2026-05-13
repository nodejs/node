// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::EastAsianTraditionalYearData;
use calendrical_calculations::{gregorian::DAYS_IN_400_YEAR_CYCLE, rata_die::RataDie};

macro_rules! day_fraction_to_ms {
    ($n:tt $(/ $d:tt)+) => {{
        Milliseconds((MILLISECONDS_IN_EPHEMERIS_DAY as i128 * $n as i128 $( / $d as i128)+) as i64)
    }};
    ($n:tt $(/ $d:tt)+, exact) => {{
        let d = day_fraction_to_ms!($n $(/ $d)+);
        assert!((d.0 as i128 $(* $d as i128)+) % MILLISECONDS_IN_EPHEMERIS_DAY as i128 == 0, "inexact");
        d
    }};
}

pub(super) const UTC_PLUS_8: Milliseconds = day_fraction_to_ms!(8 / 24);
pub(super) const UTC_PLUS_9: Milliseconds = day_fraction_to_ms!(9 / 24);
// Reference time was UTC+(1397/180)
pub(super) const BEIJING_UTC_OFFSET: Milliseconds = day_fraction_to_ms!(1397 / 180 / 24);

/// The mean year length according to the Gregorian solar cycle.
const MEAN_GREGORIAN_YEAR_LENGTH: Milliseconds =
    day_fraction_to_ms!(DAYS_IN_400_YEAR_CYCLE / 400, exact);

/// The mean solar term length according to the Gregorian solar cycle
const MEAN_GREGORIAN_SOLAR_TERM_LENGTH: Milliseconds =
    day_fraction_to_ms!(DAYS_IN_400_YEAR_CYCLE / 400 / 12, exact);

/// The mean synodic length on Jan 1 2000 according to the [Astronomical Almanac (1992)].
///
/// [Astronomical Almanac (1992)]: https://archive.org/details/131123ExplanatorySupplementAstronomicalAlmanac/page/n302/mode/1up
const MEAN_SYNODIC_MONTH_LENGTH: Milliseconds = day_fraction_to_ms!(295305888531 / 10000000000i64);

/// Number of milliseconds in a day.
const MILLISECONDS_IN_EPHEMERIS_DAY: i64 = 24 * 60 * 60 * 1000;

// 1999-12-22T07:44, https://aa.usno.navy.mil/calculated/seasons?year=2024&tz=0.00&tz_sign=-1&tz_label=false&dst=false
const UTC_SOLSTICE: LocalMoment = LocalMoment {
    rata_die: calendrical_calculations::gregorian::fixed_from_gregorian(1999, 12, 22),
    local_milliseconds: ((7 * 60) + 44) * 60 * 1000,
};

// 2000-01-06T18:14 https://aa.usno.navy.mil/calculated/moon/phases?date=2000-01-01&nump=1&format=t
const UTC_NEW_MOON: LocalMoment = LocalMoment {
    rata_die: calendrical_calculations::gregorian::fixed_from_gregorian(2000, 1, 6),
    local_milliseconds: ((18 * 60) + 14) * 60 * 1000,
};

#[derive(Debug, Copy, Clone, Default)]
pub(super) struct Milliseconds(i64);

#[derive(Debug, Copy, Clone)]
struct LocalMoment {
    rata_die: RataDie,
    local_milliseconds: u32,
}

impl core::ops::Add<Milliseconds> for LocalMoment {
    type Output = Self;

    fn add(self, Milliseconds(duration): Milliseconds) -> Self::Output {
        let temp = self.local_milliseconds as i64 + duration;
        Self {
            rata_die: self.rata_die + temp.div_euclid(MILLISECONDS_IN_EPHEMERIS_DAY),
            local_milliseconds: temp.rem_euclid(MILLISECONDS_IN_EPHEMERIS_DAY) as u32,
        }
    }
}

impl super::EastAsianTraditionalYearData {
    /// A fast approximation for the Chinese calendar, inspired by the _píngqì_ (平氣) rule
    /// used in the Ming dynasty.
    ///
    /// Stays anchored in the Gregorian calendar, even as the Gregorian calendar drifts
    /// from the seasons in the distant future and distant past.
    pub(super) fn simple(
        utc_offset: Milliseconds,
        related_iso: i32,
    ) -> EastAsianTraditionalYearData {
        /// calculates the largest moment such that moment = base_moment + n * duration lands on rata_die (< rata_die + 1)
        fn periodic_duration_on_or_before(
            rata_die: RataDie,
            base_moment: LocalMoment,
            duration: Milliseconds,
        ) -> LocalMoment {
            let diff_millis = (rata_die - base_moment.rata_die) * MILLISECONDS_IN_EPHEMERIS_DAY
                - base_moment.local_milliseconds as i64;

            let num_periods =
                (diff_millis + MILLISECONDS_IN_EPHEMERIS_DAY - 1).div_euclid(duration.0);

            let millis = base_moment.rata_die.to_i64_date() * MILLISECONDS_IN_EPHEMERIS_DAY
                + base_moment.local_milliseconds as i64
                + num_periods * duration.0;

            // Note: this is Euclidean div/rem, but this more optimized, because
            // we know that our divisor is positive
            let rata_die = millis / MILLISECONDS_IN_EPHEMERIS_DAY - (millis < 0) as i64;
            let local_milliseconds = millis % MILLISECONDS_IN_EPHEMERIS_DAY
                + (millis < 0) as i64 * MILLISECONDS_IN_EPHEMERIS_DAY;

            LocalMoment {
                rata_die: RataDie::new(rata_die),
                local_milliseconds: local_milliseconds as u32,
            }
        }

        let mut major_solar_term = periodic_duration_on_or_before(
            calendrical_calculations::iso::day_before_year(related_iso),
            UTC_SOLSTICE + utc_offset,
            MEAN_GREGORIAN_YEAR_LENGTH,
        );

        let mut new_moon = periodic_duration_on_or_before(
            major_solar_term.rata_die,
            UTC_NEW_MOON + utc_offset,
            MEAN_SYNODIC_MONTH_LENGTH,
        );

        let mut next_new_moon = new_moon + MEAN_SYNODIC_MONTH_LENGTH;

        // The solstice is in the month of the 11th solar term of the previous year
        let mut solar_term = -2;
        let mut had_leap_in_sui = false;

        // Skip the months before the year (M11, maybe M11L, M12, maybe M12L)
        while solar_term < 0
            || (next_new_moon.rata_die <= major_solar_term.rata_die && !had_leap_in_sui)
        {
            if next_new_moon.rata_die <= major_solar_term.rata_die && !had_leap_in_sui {
                had_leap_in_sui = true;
            } else {
                solar_term += 1;
                major_solar_term = major_solar_term + MEAN_GREGORIAN_SOLAR_TERM_LENGTH;
            }

            (new_moon, next_new_moon) = (next_new_moon, next_new_moon + MEAN_SYNODIC_MONTH_LENGTH);
        }

        debug_assert_eq!(solar_term, 0);

        let start_day = new_moon.rata_die;
        let mut month_lengths = [false; 13];
        let mut leap_month = None;

        // Iterate over the 12 solar terms, producing potentially 13 months
        while solar_term < 12
            || (next_new_moon.rata_die <= major_solar_term.rata_die && !had_leap_in_sui)
        {
            *month_lengths
                .get_mut(solar_term as usize + leap_month.is_some() as usize)
                .unwrap_or(&mut false) = next_new_moon.rata_die - new_moon.rata_die == 30;

            if next_new_moon.rata_die <= major_solar_term.rata_die && !had_leap_in_sui {
                had_leap_in_sui = true;
                leap_month = Some(solar_term as u8 + 1);
            } else {
                solar_term += 1;
                major_solar_term = major_solar_term + MEAN_GREGORIAN_SOLAR_TERM_LENGTH;
            }

            (new_moon, next_new_moon) = (next_new_moon, next_new_moon + MEAN_SYNODIC_MONTH_LENGTH);
        }

        debug_assert_eq!(solar_term, 12);

        EastAsianTraditionalYearData::new(related_iso, start_day, month_lengths, leap_month)
    }
}

#[test]
fn bounds() {
    EastAsianTraditionalYearData::simple(UTC_PLUS_9, 292_277_025);
    assert!(
        std::panic::catch_unwind(|| EastAsianTraditionalYearData::simple(UTC_PLUS_9, 292_277_026))
            .is_err()
    );

    EastAsianTraditionalYearData::simple(BEIJING_UTC_OFFSET, -292_275_024);
    assert!(
        std::panic::catch_unwind(|| EastAsianTraditionalYearData::simple(
            BEIJING_UTC_OFFSET,
            -292_275_025
        ))
        .is_err()
    );
}
