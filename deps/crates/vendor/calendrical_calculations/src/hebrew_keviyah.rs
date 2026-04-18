// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

// The algorithms in this file are rather well-published in multiple places,
// though the resource that was primarily used was
// J Jean Adler's _A Short History of the Jewish Fixed Calendar_, found
// at <https://hakirah.org/vol20Ajdler.pdf>, with more detailed appendices
// at <https://www.hakirah.org/vol20AjdlerAppendices.pdf>.
// Most of the math can be found on Wikipedia as well,
// at <https://en.wikipedia.org/wiki/Hebrew_calendar#The_four_gates>

//! Alternate, more efficient structures for working with the Hebrew Calendar
//! using the keviyah and Four Gates system
//!
//! The main entry point for this code is [`YearInfo::compute_for()`] and [`YearInfo::year_containing_rd()`],
//! which will efficiently calculate certain information about a Hebrew year, given the Hebrew year
//! or a date that falls within it, and produce it as a [`YearInfo`].
//!
//! From there, you can compute additional information via [`YearInfo::new_year()`] and by accessing
//! the methods on [`YearInfo::keviyah`].
//!
//!
//! # How this code works:
//!
//! ## How the Hebrew Calendar works
//!
//! The Hebrew calendar is a lunisolar calendar following a Metonic cycle: every 19 years, the pattern of
//! leap years repeats. However, the precise month lengths vary from cycle to cycle: There are a handful of
//! corrections performed to ensure that:
//!
//! - The lunar conjunction happens on or before the first day of the month
//! - Yom Kippur is not before or after the Sabbath
//! - Hoshana Rabbah is not on the Sabbath
//!
//! These corrections can be done using systematic calculations, which this code attempts to efficiently perform.
//!
//! ## Molad
//!
//! A molad is the time of a conjunction, the moment when the new moon occurs. The "Molad Tishrei" is
//! the conjunction corresponding to the month Tishrei, the first month, so it is the molad that starts the new year.
//! In this file we'll typically use "molad" to refer to the molad Tishrei of a year.
//!
//! The Hebrew calendar does not always start on the day of the molad Tishrei: it may be postponed one or two days.
//! However, the time in the week that the molad occurs is sufficient to know when it gets postponed to.
//!
//! ## Keviyah
//!
//! See also: the [`Keviyah`] type.
//!
//! This is the core concept being used here. Everything you need to know about the characteristics
//! of a hebrew year can be boiled down to a notion called the "keviyah" of a year. This
//! encapsulates three bits of information:
//!
//! - What day of the week the year starts on
//! - What the month lengths are
//! - What day of the week Passover starts on.
//!
//! While this seems like many possible combinations, only fourteen of them are possible.
//!
//! Knowing the Keviyah of the year you can understand exactly what the lengths of each month are.
//! Furthermore, if you know the week the year falls in, you can additionally understand what
//! the precise day of the new year is.
//!
//! [`YearInfo`] encapsulates these two pieces of information: the [`Keviyah`] and the number of weeks
//! since the epoch of the Hebrew calendar.
//!
//! ## The Four Gates table
//!
//! This is an efficient lookup based way of calculating the [`Keviyah`] for a year. In the Metonic cycle,
//! there are four broad types of year: leap years, years preceding leap years, years succeeding leap years,
//! and years sandwiched between leap years. For each of these year types, there is a partitioning
//! of the week into seven segments, and the [`Keviyah`] of that year depends on which segment the molad falls
//! in.
//!
//! So to calculate the [`Keviyah`] of a year, we can calculate its molad, pick the right partitioning based on the
//! year type, and see where the molad falls in that table.

use crate::helpers::i64_to_i32;
use crate::rata_die::RataDie;
use core::cmp::Ordering;

// A note on time notation
//
// Hebrew timekeeping has some differences from standard timekeeping. A Hebrew day is split into 24
// hours, each split into 1080 ḥalakim ("parts", abbreviated "ḥal" or "p"). Furthermore, the Hebrew
// day for calendrical purposes canonically starts at 6PM the previous evening, e.g. Hebrew Monday
// starts on Sunday 6PM. (For non-calendrical observational purposes this varies and is based on
// the sunset, but that is not too relevant for the algorithms here.)
//
// In this file an unqualified day of the week will refer to a standard weekday, and Hebrew weekdays
// will be referred to as "Hebrew Sunday" etc. Sometimes the term "regular" or "standard" will be used
// to refer to a standard weekday when we particularly wish to avoid ambiguity.
//
// Hebrew weeks start on Sunday. A common notation for times of the week looks like 2-5-204, which
// means "second Hebrew Day of the week, 5h 204 ḥal", which is 5h 204 ḥal after the start of Hebrew
// Monday (which is 23h:204ḥal on standard Sunday).
//
// Some resources will use ḥalakim notation when talking about time during a standard day. This
// document will use standard `:` notation for this, as used above with 23h:204ḥal being equal to
// 5h 204ḥal. In other words, if a time is notated using dashes or spaces, it is relative to the
// hebrew start of day, whereas if it is notated using a colon, it is relative to midnight.
//
// Finally, Adjler, the resource we are using, uses both inclusive and exclusive time notation. It
// is typical across resources using the 2-5-204 notation for the 2 to be "second day" as opposed
// to "two full days have passed" (i.e., on the first day). However *in the context of
// calculations* Adjler will use 1-5-204 to refer to stuff happening on Hebrew Monday, and clarify
// it as (2)-5-204. This is because zero-indexing works better in calculations.
//
// Comparing these algorithms with the source in Adjler should be careful about this. All other
// resources seem to universally 1-index in the dashes notation. This file will only use
// zero-indexed notation when explicitly disambiguated, usually when talking about intervals.

/// Calculate the number of months preceding the molad Tishrei for a given hebrew year (Tishrei is the first month)
#[inline]
fn months_preceding_molad(h_year: i32) -> i64 {
    // Ft = INT((235N + 1 / 19))
    // Where N = h_year - 1 (number of elapsed years since epoch)
    // This math essentially comes from the Metonic cycle of 19 years containing
    // 235 months: 12 months per year, plus an extra month for each of the 7 leap years.

    (235 * (i64::from(h_year) - 1) + 1).div_euclid(19)
}

/// Conveniently create a constant for a ḥalakim (by default in 1-indexed notation). Produces a constant
/// that tracks the number of ḥalakim since the beginning of the week
macro_rules! ḥal {
    ($d:literal-$h:literal-$p:literal) => {{
        const CONSTANT: i32 = (($d - 1) * 24 + $h) * 1080 + $p;
        CONSTANT
    }};
    (0-indexed $d:literal-$h:literal-$p:literal) => {{
        const CONSTANT: i32 = ($d * 24 + $h) * 1080 + $p;
        CONSTANT
    }};
}

/// The molad Beherad is the first molad, i.e. the molad of the epoch year.
/// It occurred on Oct 6, 3761 BC, 23h:204ḥal (Jerusalem Time, Julian Calendar)
///
/// Which is the second Hebrew day of the week (Hebrew Monday), 5h 204ḥal, 2-5-204.
/// ("Beharad" בהרד is just a way of writing 2-5-204, ב-ה-רד using Hebrew numerals)
///
/// This is 31524ḥal after the start of the week (Saturday 6PM)
///
/// From Adjler Appendix A
const MOLAD_BEHERAD_OFFSET: i32 = ḥal!(2 - 5 - 204);

/// The amount of time a Hebrew lunation takes (in ḥalakim). This is not exactly the amount of time
/// taken by one revolution of the moon (the real world seldom has events that are perfect integer
/// multiples of 1080ths of an hour), but it is what the Hebrew calendar uses. This does mean that
/// there will be drift over time with the actual state of the celestial sphere, however that is
/// irrelevant since the actual state of the celestial sphere is not what is used for the Hebrew
/// calendar.
///
/// This is 29-12-793 in zero-indexed notation. It is equal to 765433ḥal.
/// From Adjler Appendix A
const HEBREW_LUNATION_TIME: i32 = ḥal!(0-indexed 29-12-793);

/// From Reingold (ch 8.2, in implementation for fixed-from-hebrew)
const HEBREW_APPROX_YEAR_LENGTH: f64 = 35975351.0 / 98496.0;

/// The number of ḥalakim in a week
///
/// (This is 181440)
const ḤALAKIM_IN_WEEK: i64 = 1080 * 24 * 7;

/// The Hebrew calendar epoch. It did not need to be postponed, so it occurs on Hebrew Monday, Oct 7, 3761 BCE (Julian),
/// the same as the Molad Beherad.
///
/// (note that the molad Beherad occurs on standard Sunday, but because it happens after 6PM it is still Hebrew Monday)
const HEBREW_CALENDAR_EPOCH: RataDie = crate::julian::fixed_from_julian_book_version(-3761, 10, 7);

/// The minumum hebrew year supported by this code (this is the minimum value for i32)
pub const HEBREW_MIN_YEAR: i32 = i32::MIN;
/// The minumum R.D. supported by this code (this code will clamp outside of it)
// (this constant is verified by tests)
pub const HEBREW_MIN_RD: RataDie = RataDie::new(-784362951979);
/// The maximum hebrew year supported by this code (this is the maximum alue for i32)
// (this constant is verified by tests)
pub const HEBREW_MAX_YEAR: i32 = i32::MAX;
/// The maximum R.D. supported by this code (this is the last day in [`HEBREW_MAX_YEAR`])
// (this constant is verified by tests)
pub const HEBREW_MAX_RD: RataDie = RataDie::new(784360204356);

/// Given a Hebrew Year, returns its molad specified as:
///
/// - The number of weeks since the week of Beharad (Oct 6, 3761 BCE Julian)
/// - The number of ḥalakim since the start of the week (Hebrew Sunday, starting on Saturday at 18:00)
#[inline]
fn molad_details(h_year: i32) -> (i64, i32) {
    let months_preceding = months_preceding_molad(h_year);

    // The molad tishri expressed in parts since the beginning of the week containing Molad of Beharad
    // Formula from Adjler Appendix A
    let molad = MOLAD_BEHERAD_OFFSET as i64 + months_preceding * HEBREW_LUNATION_TIME as i64;

    // Split into quotient and remainder
    let weeks_since_beharad = molad.div_euclid(ḤALAKIM_IN_WEEK);
    let in_week = molad.rem_euclid(ḤALAKIM_IN_WEEK);

    let in_week = i32::try_from(in_week);
    debug_assert!(in_week.is_ok(), "ḤALAKIM_IN_WEEK should fit in an i32");

    (weeks_since_beharad, in_week.unwrap_or(0))
}

/// Everything about a given year. Can be conveniently packed down into an i64 if needed.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
#[allow(clippy::exhaustive_structs)] // This may change but we're fine breaking this crate
pub struct YearInfo {
    /// The Keviyah of the year
    pub keviyah: Keviyah,
    /// How many full weeks have passed since the week of Beharad
    pub weeks_since_beharad: i64,
}

impl YearInfo {
    /// Compute the YearInfo for a given year
    #[inline]
    pub fn compute_for(h_year: i32) -> Self {
        let (mut weeks_since_beharad, ḥalakim) = molad_details(h_year);

        let cycle_type = MetonicCycleType::for_h_year(h_year);

        let keviyah = keviyah_for(cycle_type, ḥalakim);

        // The last six hours of Hebrew Saturday (i.e. after noon on Regular Saturday)
        // get unconditionally postponed to Monday according to the Four Gates table. This
        // puts us in a new week!
        if ḥalakim >= ḥal!(7 - 18 - 0) {
            weeks_since_beharad += 1;
        }

        Self {
            keviyah,
            weeks_since_beharad,
        }
    }

    /// Returns the YearInfo and h_year for the year containing `date`
    ///
    /// This will clamp the R.D. such that the hebrew year is within range for i32
    #[inline]
    pub fn year_containing_rd(date: RataDie) -> (Self, i32) {
        #[allow(unused_imports)]
        use core_maths::*;

        let date = date.clamp(HEBREW_MIN_RD, HEBREW_MAX_RD);

        let days_since_epoch = (date - HEBREW_CALENDAR_EPOCH) as f64;
        let maybe_approx =
            i64_to_i32(1 + days_since_epoch.div_euclid(HEBREW_APPROX_YEAR_LENGTH) as i64);
        let approx = maybe_approx.unwrap_or_else(|e| e.saturate());

        let yi = Self::compute_for(approx);

        // compute if yi ⩼ rd
        let cmp = yi.compare(date);

        let (yi, h_year) = match cmp {
            // The approx year is a year greater. Go one year down
            Ordering::Greater => {
                let prev = approx.saturating_sub(1);
                (Self::compute_for(prev), prev)
            }
            // Bullseye
            Ordering::Equal => (yi, approx),
            // The approx year is a year lower. Go one year up.
            Ordering::Less => {
                let next = approx.saturating_add(1);
                (Self::compute_for(next), next)
            }
        };

        debug_assert!(yi.compare(date).is_eq(),
                      "Date {date:?} calculated approximately to Hebrew Year {approx} (comparison: {cmp:?}), \
                       should be contained in adjacent year {h_year} but that year is still {:?} it", yi.compare(date));

        (yi, h_year)
    }

    /// Compare this year against a date. Returns Ordering::Greater
    /// when this year is after the given date
    ///
    /// i.e. this is computing self ⩼ rd
    fn compare(self, rd: RataDie) -> Ordering {
        let ny = self.new_year();
        let len = self.keviyah.year_length();

        if rd < ny {
            Ordering::Greater
        } else if rd >= ny + len.into() {
            Ordering::Less
        } else {
            Ordering::Equal
        }
    }

    /// Compute the date of New Year's Day
    #[inline]
    pub fn new_year(self) -> RataDie {
        // Beharad started on Monday
        const BEHARAD_START_OF_YEAR: StartOfYear = StartOfYear::Monday;
        let days_since_beharad = (self.weeks_since_beharad * 7)
            + self.keviyah.start_of_year() as i64
            - BEHARAD_START_OF_YEAR as i64;
        HEBREW_CALENDAR_EPOCH + days_since_beharad
    }
}

/// The Keviyah (קביעה) of a year.
///
/// A year may be one of fourteen types, categorized by the day of
/// week of the new year (the first number, 1 = Sunday), the type of year (Deficient, Regular,
/// Complete), and the day of week of the first day of Passover. The last segment disambiguates
/// between cases that have the same first two but differ on whether they are leap years (since
/// Passover happens in Nisan, after the leap month Adar).
///
/// The discriminant values of these entries are according to
/// the positions these keviyot appear in the Four Gates table,
/// with the leap year ones being offset by 7. We don't directly rely on this
/// property but it is useful for potential bitpacking, and we use it as a way
/// to double-check that the four gates code is set up correctly. We do directly
/// rely on the leap-keviyot being after the regular ones (and starting with בחה) in is_leap.
///
/// For people unsure if their editor supports bidirectional text,
/// the first Keviyah (2D3) is Bet (ב), Ḥet (ח), Gimel (ג).
///
/// (The Hebrew values are used in code for two reasons: firstly, Rust identifiers
/// can't start with a number, and secondly, sources differ on the Latin alphanumeric notation
/// but use identical Hebrew notation)
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Debug)]
#[allow(clippy::exhaustive_enums)] // There are only 14 keviyot (and always have been)
pub enum Keviyah {
    // Regular years
    /// 2D3
    בחג = 0,
    /// 2C5
    בשה = 1,
    /// 3R5
    גכה = 2,
    /// 5R7
    הכז = 3,
    /// 5C1
    השא = 4,
    /// 7D1
    זחא = 5,
    /// 7C3
    זשג = 6,

    // Leap years
    /// 2D5
    בחה = 7,
    /// 2C7
    בשז = 8,
    /// 3R7
    גכז = 9,
    /// 5D1
    החא = 10,
    /// 5C3
    השג = 11,
    /// 7D3
    זחג = 12,
    /// 7C5
    זשה = 13,
}

/// The type of year it is
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Debug)]
#[allow(clippy::exhaustive_enums)] // This is intrinsic to the calendar
pub enum YearType {
    /// חסרה: both Ḥeshvan and Kislev have 29 days
    Deficient = -1,
    /// כסדרה: Ḥeshvan has 29, Kislev has 30
    Regular = 0,
    /// שלמה: both Ḥeshvan and Kislev have 30 days
    Complete = 1,
}

impl YearType {
    /// The length correction from a regular year (354/385)
    fn length_correction(self) -> i8 {
        self as i8
    }

    /// The length of Ḥeshvan
    fn ḥeshvan_length(self) -> u8 {
        if self == Self::Complete {
            ḤESHVAN_DEFAULT_LEN + 1
        } else {
            ḤESHVAN_DEFAULT_LEN
        }
    }

    /// The length correction of Kislev
    fn kislev_length(self) -> u8 {
        if self == Self::Deficient {
            KISLEV_DEFAULT_LEN - 1
        } else {
            KISLEV_DEFAULT_LEN
        }
    }
}
/// The day of the new year. Only these four days are permitted.
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Debug)]
#[allow(clippy::exhaustive_enums)] // This is intrinsic to the calendar
pub enum StartOfYear {
    // Compiler forced me to document these, <https://en.wikipedia.org/wiki/Nowe_Ateny>
    /// Monday (everyone knows what Monday is)
    Monday = 2,
    /// Tuesday (everyone knows what Tuesday is)
    Tuesday = 3,
    /// Thursday (everyone knows what Thursday is)
    Thursday = 5,
    /// Saturday (everyone knows what Saturday is)
    Saturday = 7,
}

// Given a constant expression of the type FOO + BAR + BAZ convert
// every element into a u16 and return
macro_rules! u16_cvt(
    // the $first / $rest pattern is needed since
    // macros cannot use `+` as a separator in repetition
    ($first:ident $(+ $rest:ident)*) => {
        {
            // make sure it is constant
            // we use as here because it works in consts and in this context
            // overflow will panic anyway
            const COMPUTED: u16 = $first as u16 $(+ $rest as u16)*;
            COMPUTED
        }
    };
);

// Month lengths (ref: https://en.wikipedia.org/wiki/Hebrew_calendar#Months)
const TISHREI_LEN: u8 = 30;
// except in Complete years
const ḤESHVAN_DEFAULT_LEN: u8 = 29;
// Except in Deficient years
const KISLEV_DEFAULT_LEN: u8 = 30;
const TEVET_LEN: u8 = 29;
const SHEVAT_LEN: u8 = 30;
const ADARI_LEN: u8 = 30;
const ADAR_LEN: u8 = 29;
const NISAN_LEN: u8 = 30;
const IYYAR_LEN: u8 = 29;
const SIVAN_LEN: u8 = 30;
const TAMMUZ_LEN: u8 = 29;
const AV_LEN: u8 = 30;
const ELUL_LEN: u8 = 29;

/// Normalized month constant for Tishrei
///
/// These are not ordinal months, rather these are the month number in a regular year
/// Adar, Adar I and Adar II all normalize to 6
pub const TISHREI: u8 = 1;
/// Normalized month constant (see [`TISHREI`])
pub const ḤESHVAN: u8 = 2;
/// Normalized month constant (see [`TISHREI`])
pub const KISLEV: u8 = 3;
/// Normalized month constant (see [`TISHREI`])
pub const TEVET: u8 = 4;
/// Normalized month constant (see [`TISHREI`])
pub const SHEVAT: u8 = 5;
/// Normalized month constant (see [`TISHREI`])
pub const ADAR: u8 = 6;
/// Normalized month constant (see [`TISHREI`])
pub const NISAN: u8 = 7;
/// Normalized month constant (see [`TISHREI`])
pub const IYYAR: u8 = 8;
/// Normalized month constant (see [`TISHREI`])
pub const SIVAN: u8 = 9;
/// Normalized month constant (see [`TISHREI`])
pub const TAMMUZ: u8 = 10;
/// Normalized month constant (see [`TISHREI`])
pub const AV: u8 = 11;
/// Normalized month constant (see [`TISHREI`])
pub const ELUL: u8 = 12;

impl Keviyah {
    /// Get the type of year for this Keviyah.
    ///
    /// Comes from the second letter in this Keviyah:
    /// ח = D, כ = R, ש = C
    #[inline]
    pub fn year_type(self) -> YearType {
        match self {
            Self::בחג => YearType::Deficient,
            Self::בשה => YearType::Complete,
            Self::גכה => YearType::Regular,
            Self::הכז => YearType::Regular,
            Self::השא => YearType::Complete,
            Self::זחא => YearType::Deficient,
            Self::זשג => YearType::Complete,
            Self::בחה => YearType::Deficient,
            Self::בשז => YearType::Complete,
            Self::גכז => YearType::Regular,
            Self::החא => YearType::Deficient,
            Self::השג => YearType::Complete,
            Self::זחג => YearType::Deficient,
            Self::זשה => YearType::Complete,
        }
    }
    /// Get the day of the new year for this Keviyah
    ///
    /// Comes from the first letter in this Keviyah:
    /// ב = 2 = Monday, ג = 3 = Tuesday, ה = 5 = Thursday, ז = 7 = Saturday
    #[inline]
    pub fn start_of_year(self) -> StartOfYear {
        match self {
            Self::בחג => StartOfYear::Monday,
            Self::בשה => StartOfYear::Monday,
            Self::גכה => StartOfYear::Tuesday,
            Self::הכז => StartOfYear::Thursday,
            Self::השא => StartOfYear::Thursday,
            Self::זחא => StartOfYear::Saturday,
            Self::זשג => StartOfYear::Saturday,
            Self::בחה => StartOfYear::Monday,
            Self::בשז => StartOfYear::Monday,
            Self::גכז => StartOfYear::Tuesday,
            Self::החא => StartOfYear::Thursday,
            Self::השג => StartOfYear::Thursday,
            Self::זחג => StartOfYear::Saturday,
            Self::זשה => StartOfYear::Saturday,
        }
    }

    /// Normalize the ordinal month to the "month number" in the year (ignoring
    /// leap months), i.e. Adar and Adar II are both represented by 6.
    ///
    /// Returns None if given the index of Adar I (6 in a leap year)
    #[inline]
    fn normalized_ordinal_month(self, ordinal_month: u8) -> Option<u8> {
        if self.is_leap() {
            match ordinal_month.cmp(&6) {
                // Adar I
                Ordering::Equal => None,
                Ordering::Less => Some(ordinal_month),
                Ordering::Greater => Some(ordinal_month - 1),
            }
        } else {
            Some(ordinal_month)
        }
    }

    /// Given an ordinal, civil month (1-indexed month starting at Tishrei)
    /// return its length
    #[inline]
    pub fn month_len(self, ordinal_month: u8) -> u8 {
        // Normalize it to the month number
        let Some(normalized_ordinal_month) = self.normalized_ordinal_month(ordinal_month) else {
            return ADARI_LEN;
        };
        debug_assert!(normalized_ordinal_month <= 12 && normalized_ordinal_month > 0);
        match normalized_ordinal_month {
            TISHREI => TISHREI_LEN,
            ḤESHVAN => self.year_type().ḥeshvan_length(),
            KISLEV => self.year_type().kislev_length(),
            TEVET => TEVET_LEN,
            SHEVAT => SHEVAT_LEN,
            ADAR => ADAR_LEN,
            NISAN => NISAN_LEN,
            IYYAR => IYYAR_LEN,
            SIVAN => SIVAN_LEN,
            TAMMUZ => TAMMUZ_LEN,
            AV => AV_LEN,
            ELUL => ELUL_LEN,
            _ => {
                debug_assert!(false, "Got unknown month index {ordinal_month}");
                30
            }
        }
    }

    /// Get the number of days preceding this month
    #[inline]
    pub fn days_preceding(self, ordinal_month: u8) -> u16 {
        // convenience constant to keep the additions smallish
        // Number of days before (any) Adar in a regular year
        const BEFORE_ADAR_DEFAULT_LEN: u16 = u16_cvt!(
            TISHREI_LEN + ḤESHVAN_DEFAULT_LEN + KISLEV_DEFAULT_LEN + TEVET_LEN + SHEVAT_LEN
        );

        let Some(normalized_ordinal_month) = self.normalized_ordinal_month(ordinal_month) else {
            // Get Adar I out of the way
            let corrected =
                BEFORE_ADAR_DEFAULT_LEN as i16 + i16::from(self.year_type().length_correction());
            return corrected as u16;
        };
        debug_assert!(normalized_ordinal_month <= ELUL && normalized_ordinal_month > 0);

        let year_type = self.year_type();

        let mut days = match normalized_ordinal_month {
            TISHREI => 0,
            ḤESHVAN => u16_cvt!(TISHREI_LEN),
            KISLEV => u16_cvt!(TISHREI_LEN) + u16::from(year_type.ḥeshvan_length()),
            // Use default lengths after this, we'll apply the correction later
            // (This helps optimize this into a simple jump table)
            TEVET => u16_cvt!(TISHREI_LEN + ḤESHVAN_DEFAULT_LEN + KISLEV_DEFAULT_LEN),
            SHEVAT => u16_cvt!(TISHREI_LEN + ḤESHVAN_DEFAULT_LEN + KISLEV_DEFAULT_LEN + TEVET_LEN),
            ADAR => BEFORE_ADAR_DEFAULT_LEN,
            NISAN => u16_cvt!(BEFORE_ADAR_DEFAULT_LEN + ADAR_LEN),
            IYYAR => u16_cvt!(BEFORE_ADAR_DEFAULT_LEN + ADAR_LEN + NISAN_LEN),
            SIVAN => u16_cvt!(BEFORE_ADAR_DEFAULT_LEN + ADAR_LEN + NISAN_LEN + IYYAR_LEN),
            TAMMUZ => {
                u16_cvt!(BEFORE_ADAR_DEFAULT_LEN + ADAR_LEN + NISAN_LEN + IYYAR_LEN + SIVAN_LEN)
            }
            #[rustfmt::skip]
            AV => u16_cvt!(BEFORE_ADAR_DEFAULT_LEN + ADAR_LEN + NISAN_LEN + IYYAR_LEN + SIVAN_LEN + TAMMUZ_LEN),
            #[rustfmt::skip]
            _ => u16_cvt!(BEFORE_ADAR_DEFAULT_LEN + ADAR_LEN + NISAN_LEN + IYYAR_LEN + SIVAN_LEN + TAMMUZ_LEN + AV_LEN),
        };

        // If it is after Kislev and Ḥeshvan, we should add the year correction
        if normalized_ordinal_month > KISLEV {
            // Ensure the casts are fine
            debug_assert!(days > 1 && year_type.length_correction().abs() <= 1);
            days = (days as i16 + year_type.length_correction() as i16) as u16;
        }

        // In a leap year, after Adar (and including Adar II), we should add
        // the length of Adar 1
        if normalized_ordinal_month >= ADAR && self.is_leap() {
            days += u16::from(ADARI_LEN);
        }

        days
    }

    /// Given a day of the year, return the ordinal month and day as (month, day).
    pub fn month_day_for(self, mut day: u16) -> (u8, u8) {
        for month in 1..14 {
            let month_len = self.month_len(month);
            if let Ok(day) = u8::try_from(day) {
                if day <= month_len {
                    return (month, day);
                }
            }
            day -= u16::from(month_len);
        }
        debug_assert!(false, "Attempted to get Hebrew date for {day:?}, in keviyah {self:?}, didn't have enough days in the year");
        self.last_month_day_in_year()
    }

    /// Return the last ordinal month and day in this year as (month, day)
    #[inline]
    pub fn last_month_day_in_year(self) -> (u8, u8) {
        // Elul is always the last month of the year
        if self.is_leap() {
            (13, ELUL_LEN)
        } else {
            (12, ELUL_LEN)
        }
    }

    /// Whether this year is a leap year
    #[inline]
    pub fn is_leap(self) -> bool {
        debug_assert_eq!(Self::בחה as u8, 7, "Representation of keviyot changed!");
        // Because we have arranged our keviyot such that all leap keviyot come after
        // the regular ones, this just a comparison
        self >= Self::בחה
    }

    /// Given the hebrew year for this Keviyah, calculate the YearInfo
    #[inline]
    pub fn year_info(self, h_year: i32) -> YearInfo {
        let (mut weeks_since_beharad, ḥalakim) = molad_details(h_year);

        // The last six hours of Hebrew Saturday (i.e. after noon on Regular Saturday)
        // get unconditionally postponed to Monday according to the Four Gates table. This
        // puts us in a new week!
        if ḥalakim >= ḥal!(7 - 18 - 0) {
            weeks_since_beharad += 1;
        }

        YearInfo {
            keviyah: self,
            weeks_since_beharad,
        }
    }

    /// How many days are in this year
    #[inline]
    pub fn year_length(self) -> u16 {
        let base_year_length = if self.is_leap() { 384 } else { 354 };

        (base_year_length + i16::from(self.year_type().length_correction())) as u16
    }
    /// Construct this from an integer between 0 and 13
    ///
    /// Potentially useful for bitpacking
    #[inline]
    pub fn from_integer(integer: u8) -> Self {
        debug_assert!(
            integer < 14,
            "Keviyah::from_integer() takes in a number between 0 and 13 inclusive"
        );
        match integer {
            0 => Self::בחג,
            1 => Self::בשה,
            2 => Self::גכה,
            3 => Self::הכז,
            4 => Self::השא,
            5 => Self::זחא,
            6 => Self::זשג,
            7 => Self::בחה,
            8 => Self::בשז,
            9 => Self::גכז,
            10 => Self::החא,
            11 => Self::השג,
            12 => Self::זחג,
            _ => Self::זשה,
        }
    }
}

// Four Gates Table
// ======================
//
// The Four Gates table is a table that takes the time of week of the molad
// and produces a Keviyah for the year
/// "Metonic cycle" in general refers to any 19-year repeating pattern used by lunisolar
/// calendars. The Hebrew calendar uses one where years 3, 6, 8, 11, 14, 17, 19
/// are leap years.
///
/// The Hebrew calendar further categorizes regular years as whether they come before/after/or
/// between leap years, and this is used when performing lookups.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
enum MetonicCycleType {
    /// Before a leap year (2, 5, 10, 13, 16)
    LMinusOne,
    /// After a leap year (1, 4, 9, 12, 15)
    LPlusOne,
    /// Between leap years (7. 18)
    LPlusMinusOne,
    /// Leap year (3, 6, 8, 11, 14, 17, 19)
    Leap,
}

impl MetonicCycleType {
    fn for_h_year(h_year: i32) -> Self {
        // h_year is 1-indexed, and our metonic cycle year numberings
        // are 1-indexed, so we really need to do `(h_year - 1) % 19 + 1`
        //
        // However, that is equivalent to `h_year % 19` provided you handle the
        // fact that that operation will produce 0 instead of 19.
        // Both numbers end up in our wildcard leap year arm so that's fine.
        let remainder = h_year.rem_euclid(19);
        match remainder {
            // These numbers are 1-indexed
            2 | 5 | 10 | 13 | 16 => Self::LMinusOne,
            1 | 4 | 9 | 12 | 15 => Self::LPlusOne,
            7 | 18 => Self::LPlusMinusOne,
            _ => {
                debug_assert!(matches!(remainder, 3 | 6 | 8 | 11 | 14 | 17 | 0 | 19));
                Self::Leap
            }
        }
    }
}

// The actual Four Gates tables.
//
// Each entry is a range (ending at the next entry), and it corresponds to the equivalent discriminant value of the Keviyah type.
// Leap and regular years map to different Keviyah values, however regular years all map to the same set of
// seven values, with differing ḥalakim bounds for each. The first entry in the Four Gates table straddles the end of the previous week
// and the beginning of this one.
//
// The regular-year tables only differ by their third and last entries (We may be able to write this as more compact code)
//
// You can reference these tables from https://en.wikipedia.org/wiki/Hebrew_calendar#The_four_gates
// or from Adjler (Appendix 4). Be sure to look at the Adjler table referring the "modern calendar", older tables
// use slightly different numbers.
const FOUR_GATES_LMINUSONE: [i32; 7] = [
    ḥal!(7 - 18 - 0),
    ḥal!(1 - 9 - 204),
    ḥal!(2 - 18 - 0),
    ḥal!(3 - 9 - 204),
    ḥal!(5 - 9 - 204),
    ḥal!(5 - 18 - 0),
    ḥal!(6 - 9 - 204),
];
const FOUR_GATES_LPLUSONE: [i32; 7] = [
    ḥal!(7 - 18 - 0),
    ḥal!(1 - 9 - 204),
    ḥal!(2 - 15 - 589),
    ḥal!(3 - 9 - 204),
    ḥal!(5 - 9 - 204),
    ḥal!(5 - 18 - 0),
    ḥal!(6 - 0 - 408),
];

const FOUR_GATES_LPLUSMINUSONE: [i32; 7] = [
    ḥal!(7 - 18 - 0),
    ḥal!(1 - 9 - 204),
    ḥal!(2 - 15 - 589),
    ḥal!(3 - 9 - 204),
    ḥal!(5 - 9 - 204),
    ḥal!(5 - 18 - 0),
    ḥal!(6 - 9 - 204),
];

const FOUR_GATES_LEAP: [i32; 7] = [
    ḥal!(7 - 18 - 0),
    ḥal!(1 - 20 - 491),
    ḥal!(2 - 18 - 0),
    ḥal!(3 - 18 - 0),
    ḥal!(4 - 11 - 695),
    ḥal!(5 - 18 - 0),
    ḥal!(6 - 20 - 491),
];

/// Perform the four gates calculation, giving you the Keviyah for a given year type and
/// the ḥalakim-since-beginning-of-week of its molad Tishri
#[inline]
fn keviyah_for(year_type: MetonicCycleType, ḥalakim: i32) -> Keviyah {
    let gate = match year_type {
        MetonicCycleType::LMinusOne => FOUR_GATES_LMINUSONE,
        MetonicCycleType::LPlusOne => FOUR_GATES_LPLUSONE,
        MetonicCycleType::LPlusMinusOne => FOUR_GATES_LPLUSMINUSONE,
        MetonicCycleType::Leap => FOUR_GATES_LEAP,
    };

    // Calculate the non-leap and leap keviyot for this year
    // This could potentially be made more efficient by just finding
    // the right window on `gate` and transmuting, but this unrolled loop should be fine too.
    let keviyot = if ḥalakim >= gate[0] || ḥalakim < gate[1] {
        (Keviyah::בחג, Keviyah::בחה)
    } else if ḥalakim < gate[2] {
        (Keviyah::בשה, Keviyah::בשז)
    } else if ḥalakim < gate[3] {
        (Keviyah::גכה, Keviyah::גכז)
    } else if ḥalakim < gate[4] {
        (Keviyah::הכז, Keviyah::החא)
    } else if ḥalakim < gate[5] {
        (Keviyah::השא, Keviyah::השג)
    } else if ḥalakim < gate[6] {
        (Keviyah::זחא, Keviyah::זחג)
    } else {
        (Keviyah::זשג, Keviyah::זשה)
    };

    // We have conveniently set the discriminant value of Keviyah to match the four gates index
    // Let's just assert to make sure the table above is correct.
    debug_assert!(
        keviyot.0 as u8 + 7 == keviyot.1 as u8,
        "The table above should produce matching-indexed keviyot for the leap/non-leap year"
    );
    #[cfg(debug_assertions)]
    #[expect(clippy::indexing_slicing)] // debug_assertion code
    if keviyot.0 as u8 == 0 {
        // The first entry in the gates table straddles the ends of the week
        debug_assert!(
            ḥalakim >= gate[keviyot.0 as usize] || ḥalakim < gate[(keviyot.0 as usize + 1) % 7],
            "The table above should produce the right indexed keviyah, instead found {keviyot:?} for time {ḥalakim} (year type {year_type:?})"
        );
    } else {
        // Other entries must properly bound the ḥalakim
        debug_assert!(
            ḥalakim >= gate[keviyot.0 as usize] && ḥalakim < gate[(keviyot.0 as usize + 1) % 7],
            "The table above should produce the right indexed keviyah, instead found {keviyot:?} for time {ḥalakim} (year type {year_type:?})"
        );
    }

    if year_type == MetonicCycleType::Leap {
        keviyot.1
    } else {
        keviyot.0
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::hebrew::{self, BookHebrew};

    #[test]
    fn test_consts() {
        assert_eq!(MOLAD_BEHERAD_OFFSET, 31524);
        assert_eq!(ḤALAKIM_IN_WEEK, 181440);
        // Adjler's printed value for this constant is incorrect (as confirmed by Adjler over email).
        // Adjler is correct about the value being ḥal!(0-indexed 29-12-793).
        // which matches the math used in `crate::hebrew::molad()` from Calendrical Calculations.
        //
        // The correct constant is seen in <https://en.wikibooks.org/wiki/Computer_Programming/Hebrew_calendar>
        assert_eq!(HEBREW_LUNATION_TIME, 765433);

        // Nicer to have the code be self-contained, but always worth asserting
        assert_eq!(HEBREW_CALENDAR_EPOCH, hebrew::FIXED_HEBREW_EPOCH);
    }

    #[test]
    fn test_roundtrip_days() {
        for h_year in (1..10).chain(5775..5795).chain(10000..10010) {
            let year_info = YearInfo::compute_for(h_year);
            let ny = year_info.new_year();
            for day in 1..=year_info.keviyah.year_length() {
                let offset_date = ny + i64::from(day) - 1;
                let (offset_yearinfo, offset_h_year) = YearInfo::year_containing_rd(offset_date);

                assert_eq!(
                    offset_h_year, h_year,
                    "Backcomputed h_year should be same for day {day} in Hebrew Year {h_year}"
                );
                assert_eq!(
                    offset_yearinfo, year_info,
                    "Backcomputed YearInfo should be same for day {day} in Hebrew Year {h_year}"
                );

                let (month, day2) = year_info.keviyah.month_day_for(day);

                let days_preceding = year_info.keviyah.days_preceding(month);

                assert_eq!(
                    days_preceding + u16::from(day2),
                    day,
                    "{h_year}-{month}-{day2} should round trip for day-of-year {day}"
                )
            }
        }
    }
    #[test]
    fn test_book_parity() {
        let mut last_year = None;
        for h_year in (1..100).chain(5600..5900).chain(10000..10100) {
            let book_date = BookHebrew::from_civil_date(h_year, 1, 1);
            let book_ny = BookHebrew::fixed_from_book_hebrew(book_date);
            let kv_yearinfo = YearInfo::compute_for(h_year);
            let kv_ny = kv_yearinfo.new_year();
            assert_eq!(
                book_ny,
                kv_ny,
                "Book and Keviyah-based years should match for Hebrew Year {h_year}. Got YearInfo {kv_yearinfo:?}"
            );
            let book_is_leap = BookHebrew::is_hebrew_leap_year(h_year);
            assert_eq!(
                book_is_leap,
                kv_yearinfo.keviyah.is_leap(),
                "Book and Keviyah-based years should match for Hebrew Year {h_year}. Got YearInfo {kv_yearinfo:?}"
            );

            let book_year_len = BookHebrew::days_in_book_hebrew_year(h_year);
            let book_year_type = match book_year_len {
                355 | 385 => YearType::Complete,
                354 | 384 => YearType::Regular,
                353 | 383 => YearType::Deficient,
                _ => unreachable!("Found unexpected book year len {book_year_len}"),
            };
            assert_eq!(
                book_year_type,
                kv_yearinfo.keviyah.year_type(),
                "Book and Keviyah-based years should match for Hebrew Year {h_year}. Got YearInfo {kv_yearinfo:?}"
            );

            let kv_recomputed_yearinfo = kv_yearinfo.keviyah.year_info(h_year);
            assert_eq!(
                kv_recomputed_yearinfo,
                kv_yearinfo,
                "Recomputed YearInfo should match for Hebrew Year {h_year}. Got YearInfo {kv_yearinfo:?}"
            );

            let year_len = kv_yearinfo.keviyah.year_length();

            let month_range = if kv_yearinfo.keviyah.is_leap() {
                1..14
            } else {
                1..13
            };

            let mut days_preceding = 0;

            for month in month_range {
                let kv_month_len = kv_yearinfo.keviyah.month_len(month);
                let book_date = BookHebrew::from_civil_date(h_year, month, 1);
                let book_month_len =
                    BookHebrew::last_day_of_book_hebrew_month(book_date.year, book_date.month);
                assert_eq!(kv_month_len, book_month_len, "Month lengths should be same for ordinal hebrew month {month} in year {h_year}. Got YearInfo {kv_yearinfo:?}");

                assert_eq!(days_preceding, kv_yearinfo.keviyah.days_preceding(month), "Days preceding should be the sum of preceding days for ordinal hebrew month {month} in year {h_year}. Got YearInfo {kv_yearinfo:?}");
                days_preceding += u16::from(kv_month_len);
            }

            for offset in [0, 1, 100, year_len - 100, year_len - 2, year_len - 1] {
                let offset_date = kv_ny + offset.into();
                let (offset_yearinfo, offset_h_year) = YearInfo::year_containing_rd(offset_date);

                assert_eq!(offset_h_year, h_year, "Backcomputed h_year should be same for date {offset_date:?} in Hebrew Year {h_year} (offset from ny {offset})");
                assert_eq!(offset_yearinfo, kv_yearinfo, "Backcomputed YearInfo should be same for date {offset_date:?} in Hebrew Year {h_year} (offset from ny {offset})");
            }

            if let Some((last_h_year, predicted_ny)) = last_year {
                if last_h_year + 1 == h_year {
                    assert_eq!(predicted_ny, kv_ny, "{last_h_year}'s YearInfo predicts New Year {predicted_ny:?}, which does not match current new year. Got YearInfo {kv_yearinfo:?}");
                }
            }

            last_year = Some((h_year, kv_ny + year_len.into()))
        }
    }
    #[test]
    fn test_minmax() {
        let min = YearInfo::compute_for(HEBREW_MIN_YEAR);
        let min_ny = min.new_year();
        assert_eq!(min_ny, HEBREW_MIN_RD);

        let (recomputed_yi, recomputed_y) = YearInfo::year_containing_rd(min_ny);
        assert_eq!(recomputed_y, HEBREW_MIN_YEAR);
        assert_eq!(recomputed_yi, min);

        let max = YearInfo::compute_for(HEBREW_MAX_YEAR);
        let max_ny = max.new_year();
        // -1 since max_ny is also a part of the year
        let max_last = max_ny + i64::from(max.keviyah.year_length()) - 1;
        assert_eq!(max_last, HEBREW_MAX_RD);

        let (recomputed_yi, recomputed_y) = YearInfo::year_containing_rd(max_last);
        assert_eq!(recomputed_y, HEBREW_MAX_YEAR);
        assert_eq!(recomputed_yi, max);
    }

    #[test]
    fn test_leap_agreement() {
        for year0 in -1000..1000 {
            let year1 = year0 + 1;
            let info0 = YearInfo::compute_for(year0);
            let info1 = YearInfo::compute_for(year1);
            let num_months = (info1.new_year() - info0.new_year()) / 29;
            if info0.keviyah.is_leap() {
                assert_eq!(num_months, 13, "{year0}");
            } else {
                assert_eq!(num_months, 12, "{year0}");
            }
        }
    }
    #[test]
    fn test_issue_6262() {
        // These are years where the molad ḥalakim is *exactly* ḥal!(7 - 18 - 0), we need
        // to ensure the Saturday wraparound logic works correctly

        let rds = [
            // 72036-07-10
            (26310435, 75795),
            // 189394-12-06
            (69174713, 193152),
        ];

        for (rd, expected_year) in rds {
            let rd = RataDie::new(rd);
            let (yi, year) = YearInfo::year_containing_rd(rd);
            assert_eq!(year, expected_year);

            let yi_recomputed = yi.keviyah.year_info(year);
            assert_eq!(yi, yi_recomputed);
            // Double check that these testcases are on the boundary
            let (_weeks, ḥalakim) = molad_details(year);
            assert_eq!(ḥalakim, ḥal!(7 - 18 - 0));
        }
    }
}
