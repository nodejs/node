use crate::astronomy::{self, Astronomical, MEAN_SYNODIC_MONTH, MEAN_TROPICAL_YEAR};
use crate::gregorian::{fixed_from_gregorian, gregorian_from_fixed};
use crate::helpers::i64_to_i32;
use crate::rata_die::{Moment, RataDie};
use core::num::NonZeroU8;
use core::ops::Range;
#[allow(unused_imports)]
use core_maths::*;

// Don't iterate more than 14 times (which accounts for checking for 13 months)
const MAX_ITERS_FOR_MONTHS_OF_YEAR: u8 = 14;

/// For astronomical calendars in this module, the range in which they are expected to be well-behaved.
///
/// With astronomical calendars, for dates in the far past or far future, floating point error, algorithm inaccuracies,
/// and other issues may cause the calendar algorithm to behave unexpectedly.
///
/// Our code has a number of debug assertions for various calendrical invariants (for example, lunar calendar months
/// must be 29 or 30 days), but it will turn these off outside of these ranges.
///
/// Consumers of this code are encouraged to disallow such out-of-range values; or, if allowing them, not expect too
/// much in terms of calendrical invariants. Once we have proleptic approximations of these calendars (#5778),
/// developers will be encouraged to use them when dates are out of range.
///
/// This value is not stable and may change. It's currently somewhat arbitrarily chosen to be
/// approximately ±10,000 years from 0 CE.
//
// NOTE: this value is doc(inline)d in islamic.rs; if you wish to change this consider if you wish to also
// change the value there, or if it should be split.
pub const WELL_BEHAVED_ASTRONOMICAL_RANGE: Range<RataDie> =
    RataDie::new(365 * -10_000)..RataDie::new(365 * 10_000);

/// The trait ChineseBased is used by Chinese-based calendars to perform computations shared by such calendar.
/// To do so, calendars should:
///
/// - Implement `fn location` by providing a location at which observations of the moon are recorded, which
///   may change over time (the zone is important, long, lat, and elevation are not relevant for these calculations)
/// - Define `const EPOCH` as a `RataDie` marking the start date of the era of the Calendar for internal use,
///   which may not accurately reflect how years or eras are marked traditionally or seen by end-users
pub trait ChineseBased {
    /// Given a fixed date, return the UTC offset used for observations of the new moon in order to
    /// calculate the beginning of months. For multiple Chinese-based lunar calendars, this has
    /// changed over the years, and can cause differences in calendar date.
    fn utc_offset(fixed: RataDie) -> f64;

    /// The RataDie of the beginning of the epoch used for internal computation; this may not
    /// reflect traditional methods of year-tracking or eras, since Chinese-based calendars
    /// may not track years ordinally in the same way many western calendars do.
    const EPOCH: RataDie;

    /// The name of the calendar for debugging.
    const DEBUG_NAME: &'static str;
}

/// Given an ISO year, return the extended year
#[deprecated(since = "0.2.3", note = "extended year calculation subject to removal")]
pub fn extended_from_iso<C: ChineseBased>(iso_year: i32) -> i32 {
    iso_year
        - const {
            let Ok(y) = crate::gregorian::year_from_fixed(C::EPOCH) else {
                panic!()
            };
            y - 1
        }
}
/// Given an extended year, return the ISO year
#[deprecated(since = "0.2.3", note = "extended year calculation subject to removal")]
pub fn iso_from_extended<C: ChineseBased>(extended_year: i32) -> i32 {
    extended_year
        + const {
            let Ok(y) = crate::gregorian::year_from_fixed(C::EPOCH) else {
                panic!()
            };
            y - 1
        }
}

/// A type implementing [`ChineseBased`] for the Chinese calendar
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Default, Hash)]
#[allow(clippy::exhaustive_structs)] // newtype
pub struct Chinese;

/// A type implementing [`ChineseBased`] for the Dangi (Korean) calendar
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Default, Hash)]
#[allow(clippy::exhaustive_structs)] // newtype
pub struct Dangi;

impl ChineseBased for Chinese {
    fn utc_offset(fixed: RataDie) -> f64 {
        use crate::gregorian::fixed_from_gregorian as gregorian;
        // Before 1929, local time was used, represented as UTC+(1397/180 h).
        // In 1929, China adopted a standard time zone based on 120 degrees of longitude, meaning
        // from 1929 onward, all new moon calculations are based on UTC+8h.
        if fixed < const { gregorian(1929, 1, 1) } {
            1397.0 / 180.0 / 24.0
        } else {
            8.0 / 24.0
        }
    }

    /// The equivalent first day in the Chinese calendar (based on inception of the calendar), Feb. 15, -2636
    const EPOCH: RataDie = crate::gregorian::fixed_from_gregorian(-2636, 2, 15);
    const DEBUG_NAME: &'static str = "chinese";
}

impl ChineseBased for Dangi {
    fn utc_offset(fixed: RataDie) -> f64 {
        use crate::gregorian::fixed_from_gregorian as gregorian;
        // Before 1908, local time was used, represented as UTC+(3809/450 h).
        // This changed multiple times as different standard timezones were adopted in Korea.
        // Currently, UTC+9h is used.
        if fixed < const { gregorian(1908, 4, 1) } {
            3809.0 / 450.0 / 24.0
        } else if fixed < const { gregorian(1912, 1, 1) } {
            8.5 / 24.0
        } else if fixed < const { gregorian(1954, 3, 21) } {
            9.0 / 24.0
        } else if fixed < const { gregorian(1961, 8, 10) } {
            8.5 / 24.0
        } else {
            9.0 / 24.0
        }
    }

    /// The first day in the Korean Dangi calendar (based on the founding of Gojoseon), lunar new year -2332
    const EPOCH: RataDie = crate::gregorian::fixed_from_gregorian(-2332, 2, 15);
    const DEBUG_NAME: &'static str = "dangi";
}

/// Marks the bounds of a lunar year
#[derive(Debug, Copy, Clone)]
#[allow(clippy::exhaustive_structs)] // we're comfortable making frequent breaking changes to this crate
pub struct YearBounds {
    /// The date marking the start of the current lunar year
    pub new_year: RataDie,
    /// The date marking the start of the next lunar year
    pub next_new_year: RataDie,
}

impl YearBounds {
    /// Compute the YearBounds for the lunar year (年) containing `date`,
    /// as well as the corresponding solar year (歲). Note that since the two
    /// years overlap significantly but not entirely, the solstice bounds for the solar
    /// year *may* not include `date`.
    #[inline]
    pub fn compute<C: ChineseBased>(date: RataDie) -> Self {
        let prev_solstice = winter_solstice_on_or_before::<C>(date);
        let (new_year, next_solstice) = new_year_on_or_before_fixed_date::<C>(date, prev_solstice);
        // Using 400 here since new years can be up to 390 days apart, and we add some padding
        let next_new_year = new_year_on_or_before_fixed_date::<C>(new_year + 400, next_solstice).0;

        Self {
            new_year,
            next_new_year,
        }
    }

    /// The number of days in this year
    pub fn count_days(self) -> u16 {
        let result = self.next_new_year - self.new_year;
        debug_assert!(
            ((u16::MIN as i64)..=(u16::MAX as i64)).contains(&result),
            "Days in year should be in range of u16."
        );
        result as u16
    }

    /// Whether or not this is a leap year
    pub fn is_leap(self) -> bool {
        let difference = self.next_new_year - self.new_year;
        difference > 365
    }
}

/// Get the current major solar term of a fixed date, output as an integer from 1..=12.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5273-L5281
pub(crate) fn major_solar_term_from_fixed<C: ChineseBased>(date: RataDie) -> u32 {
    let moment: Moment = date.as_moment();
    let universal = moment - C::utc_offset(date);
    let solar_longitude =
        i64_to_i32(Astronomical::solar_longitude(Astronomical::julian_centuries(universal)) as i64);
    debug_assert!(
        solar_longitude.is_ok(),
        "Solar longitude should be in range of i32"
    );
    let s = solar_longitude.unwrap_or_else(|e| e.saturate());
    let result_signed = (2 + s.div_euclid(30) - 1).rem_euclid(12) + 1;
    debug_assert!(result_signed >= 0);
    result_signed as u32
}

/// The fixed date in standard time at the observation location of the next new moon on or after a given Moment.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5329-L5338
pub(crate) fn new_moon_on_or_after<C: ChineseBased>(moment: Moment) -> RataDie {
    let new_moon_moment = Astronomical::new_moon_at_or_after(midnight::<C>(moment));
    let utc_offset = C::utc_offset(new_moon_moment.as_rata_die());
    (new_moon_moment + utc_offset).as_rata_die()
}

/// The fixed date in standard time at the observation location of the previous new moon before a given Moment.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5318-L5327
pub(crate) fn new_moon_before<C: ChineseBased>(moment: Moment) -> RataDie {
    let new_moon_moment = Astronomical::new_moon_before(midnight::<C>(moment));
    let utc_offset = C::utc_offset(new_moon_moment.as_rata_die());
    (new_moon_moment + utc_offset).as_rata_die()
}

/// Universal time of midnight at start of a Moment's day at the observation location
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5353-L5357
pub(crate) fn midnight<C: ChineseBased>(moment: Moment) -> Moment {
    moment - C::utc_offset(moment.as_rata_die())
}

/// Determines the fixed date of the lunar new year given the start of its corresponding solar year (歲), which is
/// also the winter solstice
///
/// Calls to `no_major_solar_term` have been inlined for increased efficiency.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5370-L5394
pub(crate) fn new_year_in_sui<C: ChineseBased>(prior_solstice: RataDie) -> (RataDie, RataDie) {
    // s1 is prior_solstice
    // Using 370 here since solstices are ~365 days apart
    // Both solstices should fall on December 20, 21, 22, or 23. The calendrical calculations
    // drift away from this for large positive and negative years, so we artifically bind them
    // to this range in order for other code invariants to be upheld.
    let prior_solstice = bind_winter_solstice::<C>(prior_solstice);
    let following_solstice =
        bind_winter_solstice::<C>(winter_solstice_on_or_before::<C>(prior_solstice + 370)); // s2
    let month_after_eleventh = new_moon_on_or_after::<C>((prior_solstice + 1).as_moment()); // m12
    debug_assert!(
        month_after_eleventh - prior_solstice >= 0
            || !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&prior_solstice)
    );
    let month_after_twelfth = new_moon_on_or_after::<C>((month_after_eleventh + 1).as_moment()); // m13
    let month_after_thirteenth = new_moon_on_or_after::<C>((month_after_twelfth + 1).as_moment());
    debug_assert!(
        month_after_twelfth - month_after_eleventh >= 29
            || !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&prior_solstice)
    );
    let next_eleventh_month = new_moon_before::<C>((following_solstice + 1).as_moment()); // next-m11
    let lhs_argument =
        ((next_eleventh_month - month_after_eleventh) as f64 / MEAN_SYNODIC_MONTH).round() as i64;
    let solar_term_a = major_solar_term_from_fixed::<C>(month_after_eleventh);
    let solar_term_b = major_solar_term_from_fixed::<C>(month_after_twelfth);
    let solar_term_c = major_solar_term_from_fixed::<C>(month_after_thirteenth);
    if lhs_argument == 12 && (solar_term_a == solar_term_b || solar_term_b == solar_term_c) {
        (month_after_thirteenth, following_solstice)
    } else {
        (month_after_twelfth, following_solstice)
    }
}

/// This function forces the RataDie to be on December 20, 21, 22, or 23. It was
/// created for practical considerations and is not in the text.
///
/// See: <https://github.com/unicode-org/icu4x/pull/4904>
fn bind_winter_solstice<C: ChineseBased>(solstice: RataDie) -> RataDie {
    let (gregorian_year, gregorian_month, gregorian_day) = match gregorian_from_fixed(solstice) {
        Ok(ymd) => ymd,
        Err(_) => {
            debug_assert!(false, "Solstice REALLY out of bounds: {solstice:?}");
            return solstice;
        }
    };
    let resolved_solstice = if gregorian_month < 12 || gregorian_day < 20 {
        fixed_from_gregorian(gregorian_year, 12, 20)
    } else if gregorian_day > 23 {
        fixed_from_gregorian(gregorian_year, 12, 23)
    } else {
        solstice
    };
    if resolved_solstice != solstice {
        if !(0..=4000).contains(&gregorian_year) {
            #[cfg(feature = "logging")]
            log::trace!("({}) Solstice out of bounds: {solstice:?}", C::DEBUG_NAME);
        } else {
            debug_assert!(
                false,
                "({}) Solstice out of bounds: {solstice:?}",
                C::DEBUG_NAME
            );
        }
    }
    resolved_solstice
}

/// Get the fixed date of the nearest winter solstice, in the Chinese time zone,
/// on or before a given fixed date.
///
/// This is valid for several thousand years, but it drifts for large positive
/// and negative years. See [`bind_winter_solstice`].
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5359-L5368
pub(crate) fn winter_solstice_on_or_before<C: ChineseBased>(date: RataDie) -> RataDie {
    let approx = Astronomical::estimate_prior_solar_longitude(
        astronomy::WINTER,
        midnight::<C>((date + 1).as_moment()),
    );
    let mut iters = 0;
    let mut day = Moment::new((approx.inner() - 1.0).floor());
    while iters < MAX_ITERS_FOR_MONTHS_OF_YEAR
        && astronomy::WINTER
            >= Astronomical::solar_longitude(Astronomical::julian_centuries(midnight::<C>(
                day + 1.0,
            )))
    {
        iters += 1;
        day += 1.0;
    }
    debug_assert!(
        iters < MAX_ITERS_FOR_MONTHS_OF_YEAR || !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&date),
        "Number of iterations was higher than expected"
    );
    day.as_rata_die()
}

/// Get the fixed date of the nearest Lunar New Year on or before a given fixed date.
/// This function also returns the solstice following a given date for optimization (see #3743).
///
/// To call this function you must precompute the value of the prior solstice, which
/// is the result of winter_solstice_on_or_before
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5396-L5405
pub(crate) fn new_year_on_or_before_fixed_date<C: ChineseBased>(
    date: RataDie,
    prior_solstice: RataDie,
) -> (RataDie, RataDie) {
    let new_year = new_year_in_sui::<C>(prior_solstice);
    if date >= new_year.0 {
        new_year
    } else {
        // This happens when we're at the end of the current lunar year
        // and the solstice has already happened. Thus the relevant solstice
        // for the current lunar year is the previous one, which we calculate by offsetting
        // back by a year.
        let date_in_last_sui = date - 180; // This date is in the current lunar year, but the last solar year
        let prior_solstice = winter_solstice_on_or_before::<C>(date_in_last_sui);
        new_year_in_sui::<C>(prior_solstice)
    }
}

/// Get a RataDie in the middle of a year.
///
/// This is not necessarily meant for direct use in
/// calculations; rather, it is useful for getting a RataDie guaranteed to be in a given year
/// as input for other calculations like calculating the leap month in a year.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz
/// Lisp reference code: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5469-L5475>
pub fn fixed_mid_year_from_year<C: ChineseBased>(elapsed_years: i32) -> RataDie {
    let cycle = (elapsed_years - 1).div_euclid(60) + 1;
    let year = (elapsed_years - 1).rem_euclid(60) + 1;
    C::EPOCH + ((((cycle - 1) * 60 + year - 1) as f64 + 0.5) * MEAN_TROPICAL_YEAR) as i64
}

/// Whether this year is a leap year
pub fn is_leap_year<C: ChineseBased>(year: i32) -> bool {
    let mid_year = fixed_mid_year_from_year::<C>(year);
    YearBounds::compute::<C>(mid_year).is_leap()
}

/// The last month and day in this year
pub fn last_month_day_in_year<C: ChineseBased>(year: i32) -> (u8, u8) {
    let mid_year = fixed_mid_year_from_year::<C>(year);
    let year_bounds = YearBounds::compute::<C>(mid_year);
    let last_day = year_bounds.next_new_year - 1;
    let month = if year_bounds.is_leap() { 13 } else { 12 };
    let day = last_day - new_moon_before::<C>(last_day.as_moment()) + 1;
    (month, day as u8)
}

/// Calculated the numbers of days in the given year
pub fn days_in_provided_year<C: ChineseBased>(year: i32) -> u16 {
    let mid_year = fixed_mid_year_from_year::<C>(year);
    let bounds = YearBounds::compute::<C>(mid_year);

    bounds.count_days()
}

/// chinese_based_date_from_fixed returns extra things for use in caching
#[derive(Debug)]
#[non_exhaustive]
pub struct ChineseFromFixedResult {
    /// The chinese year
    pub year: i32,
    /// The chinese month
    pub month: u8,
    /// The chinese day
    pub day: u8,
    /// The bounds of the current lunar year
    pub year_bounds: YearBounds,
    /// The index of the leap month, if any
    pub leap_month: Option<NonZeroU8>,
}

/// Get a chinese based date from a fixed date, with the related Gregorian year
///
/// Months are calculated by iterating through the dates of new moons until finding the last month which
/// does not exceed the given fixed date. The day of month is calculated by subtracting the fixed date
/// from the fixed date of the beginning of the month.
///
/// The calculation for `elapsed_years` and `month` in this function are based on code from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5414-L5459>
pub fn chinese_based_date_from_fixed<C: ChineseBased>(date: RataDie) -> ChineseFromFixedResult {
    let year_bounds = YearBounds::compute::<C>(date);
    let first_day_of_year = year_bounds.new_year;

    let year_float =
        (1.5 - 1.0 / 12.0 + ((first_day_of_year - C::EPOCH) as f64) / MEAN_TROPICAL_YEAR).floor();
    let year_int = i64_to_i32(year_float as i64);
    debug_assert!(year_int.is_ok(), "Year should be in range of i32");
    let year = year_int.unwrap_or_else(|e| e.saturate());

    let new_moon = new_moon_before::<C>((date + 1).as_moment());
    let month_i64 = ((new_moon - first_day_of_year) as f64 / MEAN_SYNODIC_MONTH).round() as i64 + 1;
    debug_assert!(
        ((u8::MIN as i64)..=(u8::MAX as i64)).contains(&month_i64),
        "Month should be in range of u8! Value {month_i64} failed for RD {date:?}"
    );
    let month = month_i64 as u8;
    let day_i64 = date - new_moon + 1;
    debug_assert!(
        ((u8::MIN as i64)..=(u8::MAX as i64)).contains(&month_i64),
        "Day should be in range of u8! Value {month_i64} failed for RD {date:?}"
    );
    let day = day_i64 as u8;
    let leap_month = if year_bounds.is_leap() {
        // This doesn't need to be checked for `None`, since `get_leap_month_from_new_year`
        // will always return a number greater than or equal to 1, and less than 14.
        NonZeroU8::new(get_leap_month_from_new_year::<C>(first_day_of_year))
    } else {
        None
    };

    ChineseFromFixedResult {
        year,
        month,
        day,
        year_bounds,
        leap_month,
    }
}

/// Given that `new_year` is the first day of a leap year, find which month in the year is a leap month.
///
/// Since the first month in which there are no major solar terms is a leap month, this function
/// cycles through months until it finds the leap month, then returns the number of that month. This
/// function assumes the date passed in is in a leap year and tests to ensure this is the case in debug
/// mode by asserting that no more than thirteen months are analyzed.
///
/// Calls to `no_major_solar_term` have been inlined for increased efficiency.
///
/// Conceptually similar to code from _Calendrical Calculations_ by Reingold & Dershowitz
/// Lisp reference code: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5443-L5450>
pub fn get_leap_month_from_new_year<C: ChineseBased>(new_year: RataDie) -> u8 {
    let mut cur = new_year;
    let mut result = 1;
    let mut solar_term = major_solar_term_from_fixed::<C>(cur);
    loop {
        let next = new_moon_on_or_after::<C>((cur + 1).as_moment());
        let next_solar_term = major_solar_term_from_fixed::<C>(next);
        if result >= MAX_ITERS_FOR_MONTHS_OF_YEAR || solar_term == next_solar_term {
            break;
        }
        cur = next;
        solar_term = next_solar_term;
        result += 1;
    }
    debug_assert!(result < MAX_ITERS_FOR_MONTHS_OF_YEAR, "The given year was not a leap year and an unexpected number of iterations occurred searching for a leap month.");
    result
}

/// Returns the number of days in the given (year, month).
///
/// In the Chinese calendar, months start at each
/// new moon, so this function finds the number of days between the new moon at the beginning of the given
/// month and the new moon at the beginning of the next month.
pub fn month_days<C: ChineseBased>(year: i32, month: u8) -> u8 {
    let mid_year = fixed_mid_year_from_year::<C>(year);
    let prev_solstice = winter_solstice_on_or_before::<C>(mid_year);
    let new_year = new_year_on_or_before_fixed_date::<C>(mid_year, prev_solstice).0;
    days_in_month::<C>(month, new_year, None).0
}

/// Returns the number of days in the given `month` after the given `new_year`.
/// Also returns the RataDie of the new moon beginning the next month.
pub fn days_in_month<C: ChineseBased>(
    month: u8,
    new_year: RataDie,
    prev_new_moon: Option<RataDie>,
) -> (u8, RataDie) {
    let approx = new_year + ((month - 1) as i64 * 29);
    let prev_new_moon = if let Some(prev_moon) = prev_new_moon {
        prev_moon
    } else {
        new_moon_before::<C>((approx + 15).as_moment())
    };
    let next_new_moon = new_moon_on_or_after::<C>((approx + 15).as_moment());
    let result = (next_new_moon - prev_new_moon) as u8;
    debug_assert!(
        result == 29 || result == 30 || !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&new_year)
    );
    (result, next_new_moon)
}

/// Given a new year, calculate the number of days in the previous year
pub fn days_in_prev_year<C: ChineseBased>(new_year: RataDie) -> u16 {
    let date = new_year - 300;
    let prev_solstice = winter_solstice_on_or_before::<C>(date);
    let (prev_new_year, _) = new_year_on_or_before_fixed_date::<C>(date, prev_solstice);
    u16::try_from(new_year - prev_new_year).unwrap_or(360)
}

/// Returns the length of each month in the year, as well as a leap month index (1-indexed) if any.
///
/// Month lengths are stored as true for 30-day, false for 29-day.
/// In the case of no leap months, month 13 will have value false.
pub fn month_structure_for_year<C: ChineseBased>(
    new_year: RataDie,
    next_new_year: RataDie,
) -> ([bool; 13], Option<u8>) {
    let mut ret = [false; 13];

    let mut current_month_start = new_year;
    let mut current_month_major_solar_term = major_solar_term_from_fixed::<C>(new_year);
    let mut leap_month_index = None;
    for i in 0u8..12 {
        let next_month_start = new_moon_on_or_after::<C>((current_month_start + 28).as_moment());
        let next_month_major_solar_term = major_solar_term_from_fixed::<C>(next_month_start);

        if next_month_major_solar_term == current_month_major_solar_term {
            leap_month_index = Some(i + 1);
        }

        let diff = next_month_start - current_month_start;
        debug_assert!(
            diff == 29 || diff == 30 || !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&new_year)
        );
        #[expect(clippy::indexing_slicing)] // array is of length 13, we iterate till i=11
        if diff == 30 {
            ret[usize::from(i)] = true;
        }

        current_month_start = next_month_start;
        current_month_major_solar_term = next_month_major_solar_term;
    }

    if current_month_start == next_new_year {
        // not all months without solar terms are leap months; they are only leap months if
        // the year can admit them
        //
        // From Reingold & Dershowitz (p 311):
        //
        // The leap month of a 13-month winter-solstice-to-winter-solstice period is the first month
        // that does not contain a major solar term — that is, the first lunar month that is wholly within a solar month.
        //
        // As such, if a month without a solar term is found in a non-leap year, we just ingnore it.
        leap_month_index = None;
    } else {
        let diff = next_new_year - current_month_start;
        debug_assert!(
            diff == 29 || diff == 30 || !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&new_year)
        );
        if diff == 30 {
            ret[12] = true;
        }
    }
    if current_month_start != next_new_year && leap_month_index.is_none() {
        leap_month_index = Some(13); // The last month is a leap month
        debug_assert!(
            major_solar_term_from_fixed::<C>(current_month_start) == current_month_major_solar_term
                || !WELL_BEHAVED_ASTRONOMICAL_RANGE.contains(&new_year),
            "A leap month is required here, but it had a major solar term!"
        );
    }

    (ret, leap_month_index)
}

/// Given the new year and a month/day pair, calculate the number of days until the first day of the given month
pub fn days_until_month<C: ChineseBased>(new_year: RataDie, month: u8) -> u16 {
    let month_approx = 28_u16.saturating_mul(u16::from(month) - 1);

    let new_moon = new_moon_on_or_after::<C>(new_year.as_moment() + (month_approx as f64));
    let result = new_moon - new_year;
    debug_assert!(((u16::MIN as i64)..=(u16::MAX as i64)).contains(&result), "Result {result} from new moon: {new_moon:?} and new year: {new_year:?} should be in range of u16!");
    result as u16
}

#[cfg(test)]
mod test {

    use super::*;
    use crate::rata_die::Moment;

    #[test]
    fn check_epochs() {
        assert_eq!(
            YearBounds::compute::<Dangi>(Dangi::EPOCH).new_year,
            Dangi::EPOCH
        );
        assert_eq!(
            YearBounds::compute::<Chinese>(Chinese::EPOCH).new_year,
            Chinese::EPOCH
        );
    }

    #[test]
    fn test_chinese_new_moon_directionality() {
        for i in (-1000..1000).step_by(31) {
            let moment = Moment::new(i as f64);
            let before = new_moon_before::<Chinese>(moment);
            let after = new_moon_on_or_after::<Chinese>(moment);
            assert!(before < after, "Chinese new moon directionality failed for Moment: {moment:?}, with:\n\tBefore: {before:?}\n\tAfter: {after:?}");
        }
    }

    #[test]
    fn test_chinese_new_year_on_or_before() {
        let fixed = crate::gregorian::fixed_from_gregorian(2023, 6, 22);
        let prev_solstice = winter_solstice_on_or_before::<Chinese>(fixed);
        let result_fixed = new_year_on_or_before_fixed_date::<Chinese>(fixed, prev_solstice).0;
        let (y, m, d) = crate::gregorian::gregorian_from_fixed(result_fixed).unwrap();
        assert_eq!(y, 2023);
        assert_eq!(m, 1);
        assert_eq!(d, 22);
    }

    fn seollal_on_or_before(fixed: RataDie) -> RataDie {
        let prev_solstice = winter_solstice_on_or_before::<Dangi>(fixed);
        new_year_on_or_before_fixed_date::<Dangi>(fixed, prev_solstice).0
    }

    #[test]
    fn test_month_structure() {
        // Mostly just tests that the assertions aren't hit
        for year in 1900..2050 {
            let fixed = crate::gregorian::fixed_from_gregorian(year, 1, 1);
            let chinese_year = chinese_based_date_from_fixed::<Chinese>(fixed);
            let (month_lengths, leap) = month_structure_for_year::<Chinese>(
                chinese_year.year_bounds.new_year,
                chinese_year.year_bounds.next_new_year,
            );

            for (i, month_is_30) in month_lengths.into_iter().enumerate() {
                if leap.is_none() && i == 12 {
                    // month_days has no defined behavior for month 13 on non-leap-years
                    continue;
                }
                let month_len = 29 + i32::from(month_is_30);
                let month_days = month_days::<Chinese>(chinese_year.year, i as u8 + 1);
                assert_eq!(
                    month_len,
                    i32::from(month_days),
                    "Month length for month {} must be the same",
                    i + 1
                );
            }
            println!(
                "{year} (chinese {}): {month_lengths:?} {leap:?}",
                chinese_year.year
            );
        }
    }

    #[test]
    fn test_seollal() {
        #[derive(Debug)]
        struct TestCase {
            gregorian_year: i32,
            gregorian_month: u8,
            gregorian_day: u8,
            expected_year: i32,
            expected_month: u8,
            expected_day: u8,
        }

        let cases = [
            TestCase {
                gregorian_year: 2024,
                gregorian_month: 6,
                gregorian_day: 6,
                expected_year: 2024,
                expected_month: 2,
                expected_day: 10,
            },
            TestCase {
                gregorian_year: 2024,
                gregorian_month: 2,
                gregorian_day: 9,
                expected_year: 2023,
                expected_month: 1,
                expected_day: 22,
            },
            TestCase {
                gregorian_year: 2023,
                gregorian_month: 1,
                gregorian_day: 22,
                expected_year: 2023,
                expected_month: 1,
                expected_day: 22,
            },
            TestCase {
                gregorian_year: 2023,
                gregorian_month: 1,
                gregorian_day: 21,
                expected_year: 2022,
                expected_month: 2,
                expected_day: 1,
            },
            TestCase {
                gregorian_year: 2022,
                gregorian_month: 6,
                gregorian_day: 6,
                expected_year: 2022,
                expected_month: 2,
                expected_day: 1,
            },
            TestCase {
                gregorian_year: 2021,
                gregorian_month: 6,
                gregorian_day: 6,
                expected_year: 2021,
                expected_month: 2,
                expected_day: 12,
            },
            TestCase {
                gregorian_year: 2020,
                gregorian_month: 6,
                gregorian_day: 6,
                expected_year: 2020,
                expected_month: 1,
                expected_day: 25,
            },
            TestCase {
                gregorian_year: 2019,
                gregorian_month: 6,
                gregorian_day: 6,
                expected_year: 2019,
                expected_month: 2,
                expected_day: 5,
            },
            TestCase {
                gregorian_year: 2018,
                gregorian_month: 6,
                gregorian_day: 6,
                expected_year: 2018,
                expected_month: 2,
                expected_day: 16,
            },
            TestCase {
                gregorian_year: 2025,
                gregorian_month: 6,
                gregorian_day: 6,
                expected_year: 2025,
                expected_month: 1,
                expected_day: 29,
            },
            TestCase {
                gregorian_year: 2026,
                gregorian_month: 8,
                gregorian_day: 8,
                expected_year: 2026,
                expected_month: 2,
                expected_day: 17,
            },
            TestCase {
                gregorian_year: 2027,
                gregorian_month: 4,
                gregorian_day: 4,
                expected_year: 2027,
                expected_month: 2,
                expected_day: 7,
            },
            TestCase {
                gregorian_year: 2028,
                gregorian_month: 9,
                gregorian_day: 21,
                expected_year: 2028,
                expected_month: 1,
                expected_day: 27,
            },
        ];

        for case in cases {
            let fixed = crate::gregorian::fixed_from_gregorian(
                case.gregorian_year,
                case.gregorian_month,
                case.gregorian_day,
            );
            let seollal = seollal_on_or_before(fixed);
            let (y, m, d) = crate::gregorian::gregorian_from_fixed(seollal).unwrap();
            assert_eq!(
                y, case.expected_year,
                "Year check failed for case: {case:?}"
            );
            assert_eq!(
                m, case.expected_month,
                "Month check failed for case: {case:?}"
            );
            assert_eq!(d, case.expected_day, "Day check failed for case: {case:?}");
        }
    }
}
