// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Persian calendar.
//!
//! ```rust
//! use icu::calendar::Date;
//!
//! let persian_date = Date::try_new_persian(1348, 10, 11)
//!     .expect("Failed to initialize Persian Date instance.");
//!
//! assert_eq!(persian_date.era_year().year, 1348);
//! assert_eq!(persian_date.month().ordinal, 10);
//! assert_eq!(persian_date.day_of_month().0, 11);
//! ```

use crate::cal::iso::{Iso, IsoDateInner};
use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::error::DateError;
use crate::{types, Calendar, Date, DateDuration, DateDurationUnit, RangeError};
use ::tinystr::tinystr;
use calendrical_calculations::helpers::I32CastError;
use calendrical_calculations::rata_die::RataDie;

/// The [Persian Calendar](https://en.wikipedia.org/wiki/Solar_Hijri_calendar)
///
/// The Persian Calendar is a solar calendar used officially by the countries of Iran and Afghanistan and many Persian-speaking regions.
/// It has 12 months and other similarities to the [`Gregorian`](super::Gregorian) Calendar.
///
/// This type can be used with [`Date`] to represent dates in this calendar.
///
/// # Era codes
///
/// This calendar uses a single era code `ap` (aliases `sh`, `hs`), with Anno Persico/Anno Persarum starting the year of the Hijra. Dates before this era use negative years.
///
/// # Month codes
///
/// This calendar supports 12 solar month codes (`"M01" - "M12"`)
#[derive(Copy, Clone, Debug, Default, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)]
pub struct Persian;

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]

/// The inner date type used for representing [`Date`]s of [`Persian`]. See [`Date`] and [`Persian`] for more details.
pub struct PersianDateInner(ArithmeticDate<Persian>);

impl CalendarArithmetic for Persian {
    type YearInfo = i32;

    fn days_in_provided_month(year: i32, month: u8) -> u8 {
        match month {
            1..=6 => 31,
            7..=11 => 30,
            12 if Self::provided_year_is_leap(year) => 30,
            12 => 29,
            _ => 0,
        }
    }

    fn months_in_provided_year(_: i32) -> u8 {
        12
    }

    fn provided_year_is_leap(p_year: i32) -> bool {
        calendrical_calculations::persian::is_leap_year(p_year)
    }

    fn days_in_provided_year(year: i32) -> u16 {
        if Self::provided_year_is_leap(year) {
            366
        } else {
            365
        }
    }

    fn last_month_day_in_provided_year(year: i32) -> (u8, u8) {
        if Self::provided_year_is_leap(year) {
            (12, 30)
        } else {
            (12, 29)
        }
    }
}

impl crate::cal::scaffold::UnstableSealed for Persian {}
impl Calendar for Persian {
    type DateInner = PersianDateInner;
    type Year = types::EraYear;

    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let year = match era {
            Some("ap" | "sh" | "hs") | None => year,
            Some(_) => return Err(DateError::UnknownEra),
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(PersianDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        PersianDateInner(
            match calendrical_calculations::persian::fast_persian_from_fixed(rd) {
                Err(I32CastError::BelowMin) => ArithmeticDate::min_date(),
                Err(I32CastError::AboveMax) => ArithmeticDate::max_date(),
                Ok((year, month, day)) => ArithmeticDate::new_unchecked(year, month, day),
            },
        )
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        calendrical_calculations::persian::fixed_from_fast_persian(
            date.0.year,
            date.0.month,
            date.0.day,
        )
    }

    fn from_iso(&self, iso: IsoDateInner) -> PersianDateInner {
        self.from_rata_die(Iso.to_rata_die(&iso))
    }

    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        Iso.from_rata_die(self.to_rata_die(date))
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        date.0.months_in_year()
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        date.0.days_in_year()
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        date.0.days_in_month()
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        date.0.offset_date(offset, &())
    }

    #[allow(clippy::field_reassign_with_default)]
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        _largest_unit: DateDurationUnit,
        _smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        date1.0.until(date2.0, _largest_unit, _smallest_unit)
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        types::EraYear {
            era: tinystr!(16, "ap"),
            era_index: Some(0),
            year: self.extended_year(date),
            ambiguity: types::YearAmbiguity::CenturyRequired,
        }
    }

    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        date.0.extended_year()
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::provided_year_is_leap(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        date.0.month()
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        date.0.day_of_year()
    }

    fn debug_name(&self) -> &'static str {
        "Persian"
    }

    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm> {
        Some(crate::preferences::CalendarAlgorithm::Persian)
    }
}

impl Persian {
    /// Constructs a new Persian Calendar
    pub fn new() -> Self {
        Self
    }
}

impl Date<Persian> {
    /// Construct new Persian Date.
    ///
    /// Has no negative years, only era is the AH/AP.
    ///
    /// ```rust
    /// use icu::calendar::Date;
    ///
    /// let date_persian = Date::try_new_persian(1392, 4, 25)
    ///     .expect("Failed to initialize Persian Date instance.");
    ///
    /// assert_eq!(date_persian.era_year().year, 1392);
    /// assert_eq!(date_persian.month().ordinal, 4);
    /// assert_eq!(date_persian.day_of_month().0, 25);
    /// ```
    pub fn try_new_persian(year: i32, month: u8, day: u8) -> Result<Date<Persian>, RangeError> {
        ArithmeticDate::new_from_ordinals(year, month, day)
            .map(PersianDateInner)
            .map(|inner| Date::from_raw(inner, Persian))
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

    static TEST_RD: [i64; 21] = [
        656786, 664224, 671401, 694799, 702806, 704424, 708842, 709409, 709580, 727274, 728714,
        739330, 739331, 744313, 763436, 763437, 764652, 775123, 775488, 775489, 1317874,
    ];

    // Test data are provided for the range 1178-3000 AP, for which
    // we know the 33-year rule, with the override table, matches the
    // astronomical calculations based on the 52.5 degrees east meridian.
    static CASES: [DateCase; 21] = [
        // First year for which 33-year rule matches the astronomical calculation
        DateCase {
            year: 1178,
            month: 1,
            day: 1,
        },
        DateCase {
            year: 1198,
            month: 5,
            day: 10,
        },
        DateCase {
            year: 1218,
            month: 1,
            day: 7,
        },
        DateCase {
            year: 1282,
            month: 1,
            day: 29,
        },
        // The beginning of the year the calendar was adopted
        DateCase {
            year: 1304,
            month: 1,
            day: 1,
        },
        DateCase {
            year: 1308,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 1320,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 1322,
            month: 1,
            day: 29,
        },
        DateCase {
            year: 1322,
            month: 7,
            day: 14,
        },
        DateCase {
            year: 1370,
            month: 12,
            day: 27,
        },
        DateCase {
            year: 1374,
            month: 12,
            day: 6,
        },
        // First day that the 2820-year rule fails
        DateCase {
            year: 1403,
            month: 12,
            day: 30,
        },
        // First Nowruz that the 2820-year rule fails
        DateCase {
            year: 1404,
            month: 1,
            day: 1,
        },
        DateCase {
            year: 1417,
            month: 8,
            day: 19,
        },
        // First day the unmodified astronomical algorithm fails
        DateCase {
            year: 1469,
            month: 12,
            day: 30,
        },
        // First Nowruz the unmodified astronomical algorithm fails
        DateCase {
            year: 1470,
            month: 1,
            day: 1,
        },
        DateCase {
            year: 1473,
            month: 4,
            day: 28,
        },
        // Last year the 33-year rule matches the modified astronomical calculation
        DateCase {
            year: 1501,
            month: 12,
            day: 29,
        },
        DateCase {
            year: 1502,
            month: 12,
            day: 29,
        },
        DateCase {
            year: 1503,
            month: 1,
            day: 1,
        },
        DateCase {
            year: 2988,
            month: 1,
            day: 1,
        },
    ];

    fn days_in_provided_year_core(year: i32) -> u16 {
        let ny =
            calendrical_calculations::persian::fixed_from_fast_persian(year, 1, 1).to_i64_date();
        let next_ny = calendrical_calculations::persian::fixed_from_fast_persian(year + 1, 1, 1)
            .to_i64_date();

        (next_ny - ny) as u16
    }

    #[test]
    fn test_persian_leap_year() {
        let mut leap_years: [i32; 21] = [0; 21];
        // These values were computed from the "Calendrical Calculations" reference code output
        let expected_values = [
            false, false, true, false, true, false, false, false, false, true, false, true, false,
            false, true, false, false, false, false, true, true,
        ];

        for (index, case) in CASES.iter().enumerate() {
            leap_years[index] = case.year;
        }
        for (year, bool) in leap_years.iter().zip(expected_values.iter()) {
            assert_eq!(Persian::provided_year_is_leap(*year), *bool);
        }
    }

    #[test]
    fn days_in_provided_year_test() {
        for case in CASES.iter() {
            assert_eq!(
                days_in_provided_year_core(case.year),
                Persian::days_in_provided_year(case.year)
            );
        }
    }

    #[test]
    fn test_rd_from_persian() {
        for (case, f_date) in CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_persian(case.year, case.month, case.day).unwrap();

            assert_eq!(date.to_rata_die().to_i64_date(), *f_date, "{case:?}");
        }
    }
    #[test]
    fn test_persian_from_rd() {
        for (case, f_date) in CASES.iter().zip(TEST_RD.iter()) {
            let date = Date::try_new_persian(case.year, case.month, case.day).unwrap();
            assert_eq!(
                Persian.from_rata_die(RataDie::new(*f_date)),
                date.inner,
                "{case:?}"
            );
        }
    }

    // From https://calendar.ut.ac.ir/Fa/News/Data/Doc/KabiseShamsi1206-1498-new.pdf
    // Plain text version at https://github.com/roozbehp/persiancalendar/blob/main/kabise.txt
    static CALENDAR_UT_AC_IR_TEST_DATA: [(i32, bool, i32, u8, u8); 293] = [
        (1206, false, 1827, 3, 22),
        (1207, false, 1828, 3, 21),
        (1208, false, 1829, 3, 21),
        (1209, false, 1830, 3, 21),
        (1210, true, 1831, 3, 21),
        (1211, false, 1832, 3, 21),
        (1212, false, 1833, 3, 21),
        (1213, false, 1834, 3, 21),
        (1214, true, 1835, 3, 21),
        (1215, false, 1836, 3, 21),
        (1216, false, 1837, 3, 21),
        (1217, false, 1838, 3, 21),
        (1218, true, 1839, 3, 21),
        (1219, false, 1840, 3, 21),
        (1220, false, 1841, 3, 21),
        (1221, false, 1842, 3, 21),
        (1222, true, 1843, 3, 21),
        (1223, false, 1844, 3, 21),
        (1224, false, 1845, 3, 21),
        (1225, false, 1846, 3, 21),
        (1226, true, 1847, 3, 21),
        (1227, false, 1848, 3, 21),
        (1228, false, 1849, 3, 21),
        (1229, false, 1850, 3, 21),
        (1230, true, 1851, 3, 21),
        (1231, false, 1852, 3, 21),
        (1232, false, 1853, 3, 21),
        (1233, false, 1854, 3, 21),
        (1234, true, 1855, 3, 21),
        (1235, false, 1856, 3, 21),
        (1236, false, 1857, 3, 21),
        (1237, false, 1858, 3, 21),
        (1238, true, 1859, 3, 21),
        (1239, false, 1860, 3, 21),
        (1240, false, 1861, 3, 21),
        (1241, false, 1862, 3, 21),
        (1242, false, 1863, 3, 21),
        (1243, true, 1864, 3, 20),
        (1244, false, 1865, 3, 21),
        (1245, false, 1866, 3, 21),
        (1246, false, 1867, 3, 21),
        (1247, true, 1868, 3, 20),
        (1248, false, 1869, 3, 21),
        (1249, false, 1870, 3, 21),
        (1250, false, 1871, 3, 21),
        (1251, true, 1872, 3, 20),
        (1252, false, 1873, 3, 21),
        (1253, false, 1874, 3, 21),
        (1254, false, 1875, 3, 21),
        (1255, true, 1876, 3, 20),
        (1256, false, 1877, 3, 21),
        (1257, false, 1878, 3, 21),
        (1258, false, 1879, 3, 21),
        (1259, true, 1880, 3, 20),
        (1260, false, 1881, 3, 21),
        (1261, false, 1882, 3, 21),
        (1262, false, 1883, 3, 21),
        (1263, true, 1884, 3, 20),
        (1264, false, 1885, 3, 21),
        (1265, false, 1886, 3, 21),
        (1266, false, 1887, 3, 21),
        (1267, true, 1888, 3, 20),
        (1268, false, 1889, 3, 21),
        (1269, false, 1890, 3, 21),
        (1270, false, 1891, 3, 21),
        (1271, true, 1892, 3, 20),
        (1272, false, 1893, 3, 21),
        (1273, false, 1894, 3, 21),
        (1274, false, 1895, 3, 21),
        (1275, false, 1896, 3, 20),
        (1276, true, 1897, 3, 20),
        (1277, false, 1898, 3, 21),
        (1278, false, 1899, 3, 21),
        (1279, false, 1900, 3, 21),
        (1280, true, 1901, 3, 21),
        (1281, false, 1902, 3, 22),
        (1282, false, 1903, 3, 22),
        (1283, false, 1904, 3, 21),
        (1284, true, 1905, 3, 21),
        (1285, false, 1906, 3, 22),
        (1286, false, 1907, 3, 22),
        (1287, false, 1908, 3, 21),
        (1288, true, 1909, 3, 21),
        (1289, false, 1910, 3, 22),
        (1290, false, 1911, 3, 22),
        (1291, false, 1912, 3, 21),
        (1292, true, 1913, 3, 21),
        (1293, false, 1914, 3, 22),
        (1294, false, 1915, 3, 22),
        (1295, false, 1916, 3, 21),
        (1296, true, 1917, 3, 21),
        (1297, false, 1918, 3, 22),
        (1298, false, 1919, 3, 22),
        (1299, false, 1920, 3, 21),
        (1300, true, 1921, 3, 21),
        (1301, false, 1922, 3, 22),
        (1302, false, 1923, 3, 22),
        (1303, false, 1924, 3, 21),
        (1304, true, 1925, 3, 21),
        (1305, false, 1926, 3, 22),
        (1306, false, 1927, 3, 22),
        (1307, false, 1928, 3, 21),
        (1308, false, 1929, 3, 21),
        (1309, true, 1930, 3, 21),
        (1310, false, 1931, 3, 22),
        (1311, false, 1932, 3, 21),
        (1312, false, 1933, 3, 21),
        (1313, true, 1934, 3, 21),
        (1314, false, 1935, 3, 22),
        (1315, false, 1936, 3, 21),
        (1316, false, 1937, 3, 21),
        (1317, true, 1938, 3, 21),
        (1318, false, 1939, 3, 22),
        (1319, false, 1940, 3, 21),
        (1320, false, 1941, 3, 21),
        (1321, true, 1942, 3, 21),
        (1322, false, 1943, 3, 22),
        (1323, false, 1944, 3, 21),
        (1324, false, 1945, 3, 21),
        (1325, true, 1946, 3, 21),
        (1326, false, 1947, 3, 22),
        (1327, false, 1948, 3, 21),
        (1328, false, 1949, 3, 21),
        (1329, true, 1950, 3, 21),
        (1330, false, 1951, 3, 22),
        (1331, false, 1952, 3, 21),
        (1332, false, 1953, 3, 21),
        (1333, true, 1954, 3, 21),
        (1334, false, 1955, 3, 22),
        (1335, false, 1956, 3, 21),
        (1336, false, 1957, 3, 21),
        (1337, true, 1958, 3, 21),
        (1338, false, 1959, 3, 22),
        (1339, false, 1960, 3, 21),
        (1340, false, 1961, 3, 21),
        (1341, false, 1962, 3, 21),
        (1342, true, 1963, 3, 21),
        (1343, false, 1964, 3, 21),
        (1344, false, 1965, 3, 21),
        (1345, false, 1966, 3, 21),
        (1346, true, 1967, 3, 21),
        (1347, false, 1968, 3, 21),
        (1348, false, 1969, 3, 21),
        (1349, false, 1970, 3, 21),
        (1350, true, 1971, 3, 21),
        (1351, false, 1972, 3, 21),
        (1352, false, 1973, 3, 21),
        (1353, false, 1974, 3, 21),
        (1354, true, 1975, 3, 21),
        (1355, false, 1976, 3, 21),
        (1356, false, 1977, 3, 21),
        (1357, false, 1978, 3, 21),
        (1358, true, 1979, 3, 21),
        (1359, false, 1980, 3, 21),
        (1360, false, 1981, 3, 21),
        (1361, false, 1982, 3, 21),
        (1362, true, 1983, 3, 21),
        (1363, false, 1984, 3, 21),
        (1364, false, 1985, 3, 21),
        (1365, false, 1986, 3, 21),
        (1366, true, 1987, 3, 21),
        (1367, false, 1988, 3, 21),
        (1368, false, 1989, 3, 21),
        (1369, false, 1990, 3, 21),
        (1370, true, 1991, 3, 21),
        (1371, false, 1992, 3, 21),
        (1372, false, 1993, 3, 21),
        (1373, false, 1994, 3, 21),
        (1374, false, 1995, 3, 21),
        (1375, true, 1996, 3, 20),
        (1376, false, 1997, 3, 21),
        (1377, false, 1998, 3, 21),
        (1378, false, 1999, 3, 21),
        (1379, true, 2000, 3, 20),
        (1380, false, 2001, 3, 21),
        (1381, false, 2002, 3, 21),
        (1382, false, 2003, 3, 21),
        (1383, true, 2004, 3, 20),
        (1384, false, 2005, 3, 21),
        (1385, false, 2006, 3, 21),
        (1386, false, 2007, 3, 21),
        (1387, true, 2008, 3, 20),
        (1388, false, 2009, 3, 21),
        (1389, false, 2010, 3, 21),
        (1390, false, 2011, 3, 21),
        (1391, true, 2012, 3, 20),
        (1392, false, 2013, 3, 21),
        (1393, false, 2014, 3, 21),
        (1394, false, 2015, 3, 21),
        (1395, true, 2016, 3, 20),
        (1396, false, 2017, 3, 21),
        (1397, false, 2018, 3, 21),
        (1398, false, 2019, 3, 21),
        (1399, true, 2020, 3, 20),
        (1400, false, 2021, 3, 21),
        (1401, false, 2022, 3, 21),
        (1402, false, 2023, 3, 21),
        (1403, true, 2024, 3, 20),
        (1404, false, 2025, 3, 21),
        (1405, false, 2026, 3, 21),
        (1406, false, 2027, 3, 21),
        (1407, false, 2028, 3, 20),
        (1408, true, 2029, 3, 20),
        (1409, false, 2030, 3, 21),
        (1410, false, 2031, 3, 21),
        (1411, false, 2032, 3, 20),
        (1412, true, 2033, 3, 20),
        (1413, false, 2034, 3, 21),
        (1414, false, 2035, 3, 21),
        (1415, false, 2036, 3, 20),
        (1416, true, 2037, 3, 20),
        (1417, false, 2038, 3, 21),
        (1418, false, 2039, 3, 21),
        (1419, false, 2040, 3, 20),
        (1420, true, 2041, 3, 20),
        (1421, false, 2042, 3, 21),
        (1422, false, 2043, 3, 21),
        (1423, false, 2044, 3, 20),
        (1424, true, 2045, 3, 20),
        (1425, false, 2046, 3, 21),
        (1426, false, 2047, 3, 21),
        (1427, false, 2048, 3, 20),
        (1428, true, 2049, 3, 20),
        (1429, false, 2050, 3, 21),
        (1430, false, 2051, 3, 21),
        (1431, false, 2052, 3, 20),
        (1432, true, 2053, 3, 20),
        (1433, false, 2054, 3, 21),
        (1434, false, 2055, 3, 21),
        (1435, false, 2056, 3, 20),
        (1436, true, 2057, 3, 20),
        (1437, false, 2058, 3, 21),
        (1438, false, 2059, 3, 21),
        (1439, false, 2060, 3, 20),
        (1440, false, 2061, 3, 20),
        (1441, true, 2062, 3, 20),
        (1442, false, 2063, 3, 21),
        (1443, false, 2064, 3, 20),
        (1444, false, 2065, 3, 20),
        (1445, true, 2066, 3, 20),
        (1446, false, 2067, 3, 21),
        (1447, false, 2068, 3, 20),
        (1448, false, 2069, 3, 20),
        (1449, true, 2070, 3, 20),
        (1450, false, 2071, 3, 21),
        (1451, false, 2072, 3, 20),
        (1452, false, 2073, 3, 20),
        (1453, true, 2074, 3, 20),
        (1454, false, 2075, 3, 21),
        (1455, false, 2076, 3, 20),
        (1456, false, 2077, 3, 20),
        (1457, true, 2078, 3, 20),
        (1458, false, 2079, 3, 21),
        (1459, false, 2080, 3, 20),
        (1460, false, 2081, 3, 20),
        (1461, true, 2082, 3, 20),
        (1462, false, 2083, 3, 21),
        (1463, false, 2084, 3, 20),
        (1464, false, 2085, 3, 20),
        (1465, true, 2086, 3, 20),
        (1466, false, 2087, 3, 21),
        (1467, false, 2088, 3, 20),
        (1468, false, 2089, 3, 20),
        (1469, true, 2090, 3, 20),
        (1470, false, 2091, 3, 21),
        (1471, false, 2092, 3, 20),
        (1472, false, 2093, 3, 20),
        (1473, false, 2094, 3, 20),
        (1474, true, 2095, 3, 20),
        (1475, false, 2096, 3, 20),
        (1476, false, 2097, 3, 20),
        (1477, false, 2098, 3, 20),
        (1478, true, 2099, 3, 20),
        (1479, false, 2100, 3, 21),
        (1480, false, 2101, 3, 21),
        (1481, false, 2102, 3, 21),
        (1482, true, 2103, 3, 21),
        (1483, false, 2104, 3, 21),
        (1484, false, 2105, 3, 21),
        (1485, false, 2106, 3, 21),
        (1486, true, 2107, 3, 21),
        (1487, false, 2108, 3, 21),
        (1488, false, 2109, 3, 21),
        (1489, false, 2110, 3, 21),
        (1490, true, 2111, 3, 21),
        (1491, false, 2112, 3, 21),
        (1492, false, 2113, 3, 21),
        (1493, false, 2114, 3, 21),
        (1494, true, 2115, 3, 21),
        (1495, false, 2116, 3, 21),
        (1496, false, 2117, 3, 21),
        (1497, false, 2118, 3, 21),
        (1498, true, 2119, 3, 21),
    ];

    #[test]
    fn test_calendar_ut_ac_ir_data() {
        for (p_year, leap, iso_year, iso_month, iso_day) in CALENDAR_UT_AC_IR_TEST_DATA.iter() {
            assert_eq!(Persian::provided_year_is_leap(*p_year), *leap);
            let persian_date = Date::try_new_persian(*p_year, 1, 1).unwrap();
            let iso_date = persian_date.to_calendar(Iso);
            assert_eq!(iso_date.era_year().year, *iso_year);
            assert_eq!(iso_date.month().ordinal, *iso_month);
            assert_eq!(iso_date.day_of_month().0, *iso_day);
        }
    }
}
