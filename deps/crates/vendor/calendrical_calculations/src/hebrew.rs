// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use crate::helpers::{final_func, i64_to_i32, next_u8};
use crate::rata_die::{Moment, RataDie};
#[allow(unused_imports)]
use core_maths::*;

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2206>
pub(crate) const FIXED_HEBREW_EPOCH: RataDie =
    crate::julian::fixed_from_julian_book_version(-3761, 10, 7);

/// Biblical Hebrew dates. The months are reckoned a bit strangely, with the new year occurring on
/// Tishri (as in the civil calendar) but the months being numbered in a different order
#[derive(Copy, Clone, Debug, Default, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)]
pub struct BookHebrew {
    /// The year
    pub year: i32,
    /// The month
    pub month: u8,
    /// The day
    pub day: u8,
}

// The BookHebrew Months
/// The biblical month number used for the month of Nisan
pub const NISAN: u8 = 1;
/// The biblical month number used for the month of Iyyar
pub const IYYAR: u8 = 2;
/// The biblical month number used for the month of Sivan
pub const SIVAN: u8 = 3;
/// The biblical month number used for the month of Tammuz
pub const TAMMUZ: u8 = 4;
/// The biblical month number used for the month of Av
pub const AV: u8 = 5;
/// The biblical month number used for the month of Elul
pub const ELUL: u8 = 6;
/// The biblical month number used for the month of Tishri
pub const TISHRI: u8 = 7;
/// The biblical month number used for the month of Marheshvan
pub const MARHESHVAN: u8 = 8;
/// The biblical month number used for the month of Kislev
pub const KISLEV: u8 = 9;
/// The biblical month number used for the month of Tevet
pub const TEVET: u8 = 10;
/// The biblical month number used for the month of Shevat
pub const SHEVAT: u8 = 11;
/// The biblical month number used for the month of Adar (and Adar I)
pub const ADAR: u8 = 12;
/// The biblical month number used for the month of Adar II
pub const ADARII: u8 = 13;

// BIBLICAL HEBREW CALENDAR FUNCTIONS

impl BookHebrew {
    /// The civil calendar has the same year and day numbering as the book one, but the months are numbered
    /// differently
    pub fn to_civil_date(self) -> (i32, u8, u8) {
        let biblical_month = self.month;
        let biblical_year = self.year;
        let mut civil_month;
        civil_month = (biblical_month + 6) % 12;

        if civil_month == 0 {
            civil_month = 12;
        }

        if Self::is_hebrew_leap_year(biblical_year) && biblical_month < TISHRI {
            civil_month += 1;
        }
        (biblical_year, civil_month, self.day)
    }

    /// The civil calendar has the same year and day numbering as the book one, but the months are numbered
    /// differently
    pub fn from_civil_date(civil_year: i32, civil_month: u8, civil_day: u8) -> Self {
        let mut biblical_month;

        if civil_month <= 6 {
            biblical_month = civil_month + 6; //  months 1-6 correspond to biblical months 7-12
        } else {
            biblical_month = civil_month - 6; //  months 7-12 correspond to biblical months 1-6
            if Self::is_hebrew_leap_year(civil_year) {
                biblical_month -= 1
            }
            if biblical_month == 0 {
                // Special case for Adar II in a leap year
                biblical_month = 13;
            }
        }

        BookHebrew {
            year: civil_year,
            month: biblical_month,
            day: civil_day,
        }
    }
    // Moment of mean conjunction (New Moon) of h_month in BookHebrew
    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2244>
    #[allow(dead_code)]
    pub(crate) fn molad(book_year: i32, book_month: u8) -> Moment {
        let y = if book_month < TISHRI {
            book_year + 1
        } else {
            book_year
        }; // Treat Nisan as start of year

        let months_elapsed = (book_month as f64 - TISHRI as f64) // Months this year
            + ((235.0 * y as f64 - 234.0) / 19.0).floor(); // Months until New Year.

        Moment::new(
            FIXED_HEBREW_EPOCH.to_f64_date() - (876.0 / 25920.0)
                + months_elapsed * (29.0 + (1.0 / 2.0) + (793.0 / 25920.0)),
        )
    }

    // ADAR = 12, ADARII = 13
    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2217>
    #[allow(dead_code)]
    fn last_month_of_book_hebrew_year(book_year: i32) -> u8 {
        if Self::is_hebrew_leap_year(book_year) {
            ADARII
        } else {
            ADAR
        }
    }

    // Number of days elapsed from the (Sunday) noon prior to the epoch of the BookHebrew Calendar to the molad of Tishri of BookHebrew year (h_year) or one day later
    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2261>
    fn book_hebrew_calendar_elapsed_days(book_year: i32) -> i32 {
        let months_elapsed = ((235.0 * book_year as f64 - 234.0) / 19.0).floor() as i64;
        let parts_elapsed = 12084 + 13753 * months_elapsed;
        let days = 29 * months_elapsed + (parts_elapsed as f64 / 25920.0).floor() as i64;

        if (3 * (days + 1)).rem_euclid(7) < 3 {
            days as i32 + 1
        } else {
            days as i32
        }
    }

    // Delays to start of BookHebrew year to keep ordinary year in range 353-356 and leap year in range 383-386
    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2301>
    fn book_hebrew_year_length_correction(book_year: i32) -> u8 {
        let ny0 = Self::book_hebrew_calendar_elapsed_days(book_year - 1);
        let ny1 = Self::book_hebrew_calendar_elapsed_days(book_year);
        let ny2 = Self::book_hebrew_calendar_elapsed_days(book_year + 1);

        if (ny2 - ny1) == 356 {
            2
        } else if (ny1 - ny0) == 382 {
            1
        } else {
            0
        }
    }

    // Fixed date of BookHebrew new year
    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2294>
    pub fn book_hebrew_new_year(book_year: i32) -> RataDie {
        RataDie::new(
            FIXED_HEBREW_EPOCH.to_i64_date()
                + Self::book_hebrew_calendar_elapsed_days(book_year) as i64
                + Self::book_hebrew_year_length_correction(book_year) as i64,
        )
    }

    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2315>
    pub fn days_in_book_hebrew_year(book_year: i32) -> u16 {
        (Self::book_hebrew_new_year(1 + book_year) - Self::book_hebrew_new_year(book_year)) as u16
    }

    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L2275-L2278>
    pub fn is_hebrew_leap_year(book_year: i32) -> bool {
        (7 * book_year + 1).rem_euclid(19) < 7
    }

    // True if the month Marheshvan is going to be long in given BookHebrew year
    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2321>
    #[allow(dead_code)]
    fn is_long_marheshvan(book_year: i32) -> bool {
        let long_marheshavan_year_lengths = [355, 385];
        long_marheshavan_year_lengths.contains(&Self::days_in_book_hebrew_year(book_year))
    }

    // True if the month Kislve is going to be short in given BookHebrew year
    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2326>
    #[allow(dead_code)]
    fn is_short_kislev(book_year: i32) -> bool {
        let short_kislev_year_lengths = [353, 383];
        short_kislev_year_lengths.contains(&Self::days_in_book_hebrew_year(book_year))
    }

    // Last day of month (h_month) in BookHebrew year (book_year)
    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2230>
    pub fn last_day_of_book_hebrew_month(book_year: i32, book_month: u8) -> u8 {
        match book_month {
            IYYAR | TAMMUZ | ELUL | TEVET | ADARII => 29,
            ADAR => {
                if !Self::is_hebrew_leap_year(book_year) {
                    29
                } else {
                    30
                }
            }
            MARHESHVAN => {
                if !Self::is_long_marheshvan(book_year) {
                    29
                } else {
                    30
                }
            }
            KISLEV => {
                if Self::is_short_kislev(book_year) {
                    29
                } else {
                    30
                }
            }
            _ => 30,
        }
    }

    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2331>
    pub fn fixed_from_book_hebrew(date: BookHebrew) -> RataDie {
        let book_year = date.year;
        let book_month = date.month;
        let book_day = date.day;

        let mut total_days = Self::book_hebrew_new_year(book_year) + book_day.into() - 1; // (day - 1) Days so far this month.

        if book_month < TISHRI {
            // Then add days in prior months this year before
            for m in
                (TISHRI..=Self::last_month_of_book_hebrew_year(book_year)).chain(NISAN..book_month)
            {
                total_days += Self::last_day_of_book_hebrew_month(book_year, m).into();
            }
        } else {
            // Else add days in prior months this year
            for m in TISHRI..book_month {
                total_days += Self::last_day_of_book_hebrew_month(book_year, m).into();
            }
        }

        total_days
    }

    /// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2352>
    pub fn book_hebrew_from_fixed(date: RataDie) -> BookHebrew {
        let approx = i64_to_i32(
            1 + ((date - FIXED_HEBREW_EPOCH) as f64).div_euclid(35975351.0 / 98496.0) as i64, //  The value 35975351/98496, the average length of a BookHebrew year, can be approximated by 365.25
        )
        .unwrap_or_else(|e| e.saturate());

        // Search forward for the year
        let year_condition = |year: i32| Self::book_hebrew_new_year(year) <= date;
        let year = final_func(approx - 1, year_condition);

        // Starting month for search for month.
        let start = if date
            < Self::fixed_from_book_hebrew(BookHebrew {
                year,
                month: NISAN,
                day: 1,
            }) {
            TISHRI
        } else {
            NISAN
        };

        let month_condition = |m: u8| {
            date <= Self::fixed_from_book_hebrew(BookHebrew {
                year,
                month: m,
                day: Self::last_day_of_book_hebrew_month(year, m),
            })
        };
        // Search forward from either Tishri or Nisan.
        let month = next_u8(start, month_condition);

        // Calculate the day by subtraction.
        let day = (date
            - Self::fixed_from_book_hebrew(BookHebrew {
                year,
                month,
                day: 1,
            }))
            + 1;

        BookHebrew {
            year,
            month,
            day: day as u8,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Debug)]
    struct DateCase {
        year: i32,
        month: u8,
        day: u8,
    }

    static TEST_FIXED_DATE: [i64; 33] = [
        -214193, -61387, 25469, 49217, 171307, 210155, 253427, 369740, 400085, 434355, 452605,
        470160, 473837, 507850, 524156, 544676, 567118, 569477, 601716, 613424, 626596, 645554,
        664224, 671401, 694799, 704424, 708842, 709409, 709580, 727274, 728714, 744313, 764652,
    ];

    static HEBREW_DATES: [DateCase; 33] = [
        DateCase {
            year: 3174,
            month: 5,
            day: 10,
        },
        DateCase {
            year: 3593,
            month: 9,
            day: 25,
        },
        DateCase {
            year: 3831,
            month: 7,
            day: 3,
        },
        DateCase {
            year: 3896,
            month: 7,
            day: 9,
        },
        DateCase {
            year: 4230,
            month: 10,
            day: 18,
        },
        DateCase {
            year: 4336,
            month: 3,
            day: 4,
        },
        DateCase {
            year: 4455,
            month: 8,
            day: 13,
        },
        DateCase {
            year: 4773,
            month: 2,
            day: 6,
        },
        DateCase {
            year: 4856,
            month: 2,
            day: 23,
        },
        DateCase {
            year: 4950,
            month: 1,
            day: 7,
        },
        DateCase {
            year: 5000,
            month: 13,
            day: 8,
        },
        DateCase {
            year: 5048,
            month: 1,
            day: 21,
        },
        DateCase {
            year: 5058,
            month: 2,
            day: 7,
        },
        DateCase {
            year: 5151,
            month: 4,
            day: 1,
        },
        DateCase {
            year: 5196,
            month: 11,
            day: 7,
        },
        DateCase {
            year: 5252,
            month: 1,
            day: 3,
        },
        DateCase {
            year: 5314,
            month: 7,
            day: 1,
        },
        DateCase {
            year: 5320,
            month: 12,
            day: 27,
        },
        DateCase {
            year: 5408,
            month: 3,
            day: 20,
        },
        DateCase {
            year: 5440,
            month: 4,
            day: 3,
        },
        DateCase {
            year: 5476,
            month: 5,
            day: 5,
        },
        DateCase {
            year: 5528,
            month: 4,
            day: 4,
        },
        DateCase {
            year: 5579,
            month: 5,
            day: 11,
        },
        DateCase {
            year: 5599,
            month: 1,
            day: 12,
        },
        DateCase {
            year: 5663,
            month: 1,
            day: 22,
        },
        DateCase {
            year: 5689,
            month: 5,
            day: 19,
        },
        DateCase {
            year: 5702,
            month: 7,
            day: 8,
        },
        DateCase {
            year: 5703,
            month: 1,
            day: 14,
        },
        DateCase {
            year: 5704,
            month: 7,
            day: 8,
        },
        DateCase {
            year: 5752,
            month: 13,
            day: 12,
        },
        DateCase {
            year: 5756,
            month: 12,
            day: 5,
        },
        DateCase {
            year: 5799,
            month: 8,
            day: 12,
        },
        DateCase {
            year: 5854,
            month: 5,
            day: 5,
        },
    ];

    static EXPECTED_MOLAD_DATES: [f64; 33] = [
        -1850718767f64 / 8640f64,
        -1591805959f64 / 25920f64,
        660097927f64 / 25920f64,
        1275506059f64 / 25920f64,
        4439806081f64 / 25920f64,
        605235101f64 / 2880f64,
        3284237627f64 / 12960f64,
        9583515841f64 / 25920f64,
        2592403883f64 / 6480f64,
        2251656649f64 / 5184f64,
        11731320839f64 / 25920f64,
        12185988041f64 / 25920f64,
        6140833583f64 / 12960f64,
        6581722991f64 / 12960f64,
        6792982499f64 / 12960f64,
        4705980311f64 / 8640f64,
        14699670013f64 / 25920f64,
        738006961f64 / 1296f64,
        1949499007f64 / 3240f64,
        5299956319f64 / 8640f64,
        3248250415f64 / 5184f64,
        16732660061f64 / 25920f64,
        17216413717f64 / 25920f64,
        1087650871f64 / 1620f64,
        2251079609f64 / 3240f64,
        608605601f64 / 864f64,
        306216383f64 / 432f64,
        18387526207f64 / 25920f64,
        3678423761f64 / 5184f64,
        1570884431f64 / 2160f64,
        18888119389f64 / 25920f64,
        19292268013f64 / 25920f64,
        660655045f64 / 864f64,
    ];

    static EXPECTED_LAST_HEBREW_MONTH: [u8; 33] = [
        12, 12, 12, 12, 12, 12, 12, 12, 13, 12, 13, 12, 12, 12, 12, 13, 12, 13, 12, 13, 12, 12, 12,
        12, 12, 13, 12, 13, 12, 13, 12, 12, 12,
    ];

    static EXPECTED_HEBREW_ELASPED_CALENDAR_DAYS: [i32; 33] = [
        1158928, 1311957, 1398894, 1422636, 1544627, 1583342, 1626812, 1742956, 1773254, 1807597,
        1825848, 1843388, 1847051, 1881010, 1897460, 1917895, 1940545, 1942729, 1974889, 1986554,
        1999723, 2018712, 2037346, 2044640, 2068027, 2077507, 2082262, 2082617, 2083000, 2100511,
        2101988, 2117699, 2137779,
    ];

    static EXPECTED_FIXED_HEBREW_NEW_YEAR: [i64; 33] = [
        -214497, -61470, 25467, 49209, 171200, 209915, 253385, 369529, 399827, 434172, 452421,
        469963, 473624, 507583, 524033, 544468, 567118, 569302, 601462, 613127, 626296, 645285,
        663919, 671213, 694600, 704080, 708835, 709190, 709573, 727084, 728561, 744272, 764352,
    ];

    static EXPECTED_DAYS_IN_HEBREW_YEAR: [u16; 33] = [
        354, 354, 355, 355, 355, 355, 355, 353, 383, 354, 383, 354, 354, 355, 353, 383, 353, 385,
        353, 383, 355, 354, 354, 354, 355, 385, 355, 383, 354, 385, 355, 354, 355,
    ];

    static EXPECTED_MARHESHVAN_VALUES: [bool; 33] = [
        false, false, true, true, true, true, true, false, false, false, false, false, false, true,
        false, false, false, true, false, false, true, false, false, false, true, true, true,
        false, false, true, true, false, true,
    ];

    static EXPECTED_KISLEV_VALUES: [bool; 33] = [
        false, false, false, false, false, false, false, true, true, false, true, false, false,
        false, true, true, true, false, true, true, false, false, false, false, false, false,
        false, true, false, false, false, false, false,
    ];

    static EXPECTED_DAY_IN_MONTH: [u8; 33] = [
        30, 30, 30, 30, 29, 30, 30, 29, 29, 30, 29, 30, 29, 29, 30, 30, 30, 30, 30, 29, 30, 29, 30,
        30, 30, 30, 30, 30, 30, 29, 29, 29, 30,
    ];

    #[allow(dead_code)]
    static CIVIL_EXPECTED_DAY_IN_MONTH: [u8; 33] = [
        30, 30, 30, 30, 29, 30, 29, 29, 29, 30, 30, 30, 29, 29, 30, 29, 30, 29, 29, 30, 30, 29, 30,
        30, 30, 30, 30, 29, 30, 30, 29, 29, 30,
    ];

    #[test]
    fn test_hebrew_epoch() {
        // page 119 of the Calendrical Calculations book
        assert_eq!(FIXED_HEBREW_EPOCH, RataDie::new(-1373427));
    }

    #[test]
    fn test_hebrew_molad() {
        let precision = 1_00000f64;
        for (case, expected) in HEBREW_DATES.iter().zip(EXPECTED_MOLAD_DATES.iter()) {
            let molad =
                (BookHebrew::molad(case.year, case.month).inner() * precision).round() / precision;
            let final_expected = (expected * precision).round() / precision;
            assert_eq!(molad, final_expected, "{case:?}");
        }
    }

    #[test]
    fn test_last_book_hebrew_month() {
        for (case, expected) in HEBREW_DATES.iter().zip(EXPECTED_LAST_HEBREW_MONTH.iter()) {
            let last_month = BookHebrew::last_month_of_book_hebrew_year(case.year);
            assert_eq!(last_month, *expected);
        }
    }

    #[test]
    fn test_book_hebrew_calendar_elapsed_days() {
        for (case, expected) in HEBREW_DATES
            .iter()
            .zip(EXPECTED_HEBREW_ELASPED_CALENDAR_DAYS.iter())
        {
            let elapsed_days = BookHebrew::book_hebrew_calendar_elapsed_days(case.year);
            assert_eq!(elapsed_days, *expected);
        }
    }

    #[test]
    fn test_book_hebrew_year_length_correction() {
        let year_length_correction: [u8; 33] = [
            2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0,
        ];
        for (case, expected) in HEBREW_DATES.iter().zip(year_length_correction.iter()) {
            let correction = BookHebrew::book_hebrew_year_length_correction(case.year);
            assert_eq!(correction, *expected);
        }
    }

    #[test]
    fn test_book_hebrew_new_year() {
        for (case, expected) in HEBREW_DATES
            .iter()
            .zip(EXPECTED_FIXED_HEBREW_NEW_YEAR.iter())
        {
            let f_date = BookHebrew::book_hebrew_new_year(case.year);
            assert_eq!(f_date.to_i64_date(), *expected);
        }
    }

    #[test]
    fn test_days_in_book_hebrew_year() {
        for (case, expected) in HEBREW_DATES.iter().zip(EXPECTED_DAYS_IN_HEBREW_YEAR.iter()) {
            let days_in_year = BookHebrew::days_in_book_hebrew_year(case.year);
            assert_eq!(days_in_year, *expected);
        }
    }

    #[test]
    fn test_long_marheshvan() {
        for (case, expected) in HEBREW_DATES.iter().zip(EXPECTED_MARHESHVAN_VALUES.iter()) {
            let marsheshvan = BookHebrew::is_long_marheshvan(case.year);
            assert_eq!(marsheshvan, *expected);
        }
    }

    #[test]
    fn test_short_kislev() {
        for (case, expected) in HEBREW_DATES.iter().zip(EXPECTED_KISLEV_VALUES.iter()) {
            let kislev = BookHebrew::is_short_kislev(case.year);
            assert_eq!(kislev, *expected);
        }
    }

    #[test]
    fn test_last_day_in_book_hebrew_month() {
        for (case, expected) in HEBREW_DATES.iter().zip(EXPECTED_DAY_IN_MONTH.iter()) {
            let days_in_month = BookHebrew::last_day_of_book_hebrew_month(case.year, case.month);
            assert_eq!(days_in_month, *expected);
        }
    }

    #[test]
    fn test_fixed_from_book_hebrew() {
        for (case, f_date) in HEBREW_DATES.iter().zip(TEST_FIXED_DATE.iter()) {
            assert_eq!(
                BookHebrew::fixed_from_book_hebrew(BookHebrew {
                    year: case.year,
                    month: case.month,
                    day: case.day
                }),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_book_hebrew_from_fixed() {
        for (case, f_date) in HEBREW_DATES.iter().zip(TEST_FIXED_DATE.iter()) {
            assert_eq!(
                BookHebrew::book_hebrew_from_fixed(RataDie::new(*f_date)),
                BookHebrew {
                    year: case.year,
                    month: case.month,
                    day: case.day
                },
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_civil_to_book_conversion() {
        for (f_date, case) in TEST_FIXED_DATE.iter().zip(HEBREW_DATES.iter()) {
            let book_hebrew = BookHebrew::book_hebrew_from_fixed(RataDie::new(*f_date));
            let (y, m, d) = book_hebrew.to_civil_date();
            let book_hebrew = BookHebrew::from_civil_date(y, m, d);

            assert_eq!(
                (case.year, case.month),
                (book_hebrew.year, book_hebrew.month)
            )
        }
    }
}
