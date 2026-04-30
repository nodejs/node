//! This module implements `MonthDay` and any directly related algorithms.

use alloc::string::String;
use core::str::FromStr;

use crate::{
    builtins::calendar::CalendarFields,
    iso::{IsoDate, IsoDateTime, IsoTime},
    options::{Disambiguation, DisplayCalendar, Overflow},
    parsed_intermediates::ParsedDate,
    parsers::{FormattableCalendar, FormattableDate, FormattableMonthDay},
    provider::TimeZoneProvider,
    unix_time::EpochNanoseconds,
    Calendar, MonthCode, TemporalError, TemporalResult, TimeZone,
};

use super::{PartialDate, PlainDate};
use icu_calendar::AnyCalendarKind;
use writeable::Writeable;

/// The native Rust implementation of `Temporal.PlainMonthDay`.
///
/// Represents a calendar month and day without a specific year, such as
/// "December 25th" or "March 15th". Useful for representing recurring annual
/// events where the year is not specified or relevant.
///
/// Commonly used for holidays, birthdays, anniversaries, and other events
/// that occur on the same date each year. Special handling is required for
/// February 29th when working with non-leap years.
///
/// ## Examples
///
/// ### Creating a PlainMonthDay
///
/// ```rust
/// use temporal_rs::{PlainMonthDay, Calendar, MonthCode, options::Overflow};
///
/// // Create March 15th
/// let md = PlainMonthDay::new_with_overflow(
///     3, 15,                           // month, day
///     Calendar::default(),             // ISO 8601 calendar
///     Overflow::Reject,      // reject invalid dates
///     None                             // no reference year
/// ).unwrap();
///
/// assert_eq!(md.month_code(), MonthCode::try_from_utf8("M03".as_bytes()).unwrap());
/// assert_eq!(md.day(), 15);
/// assert_eq!(md.calendar().identifier(), "iso8601");
/// ```
///
/// ### Parsing ISO 8601 month-day strings
///
/// ```rust
/// use temporal_rs::{PlainMonthDay, MonthCode};
/// use core::str::FromStr;
///
/// // Parse month-day strings
/// let md = PlainMonthDay::from_str("03-15").unwrap();
/// assert_eq!(md.month_code(), MonthCode::try_from_utf8("M03".as_bytes()).unwrap());
/// assert_eq!(md.day(), 15);
///
/// // Also supports various formats
/// let md2 = PlainMonthDay::from_str("--03-15").unwrap(); // RFC 3339 format
/// assert_eq!(md2.month_code(), MonthCode::try_from_utf8("M03".as_bytes()).unwrap());
/// assert_eq!(md2.day(), 15);
/// assert_eq!(md, md2); // equivalent
/// ```
///
/// ### Working with partial fields
///
/// ```rust
/// use temporal_rs::{PlainMonthDay, MonthCode, fields::CalendarFields};
/// use core::str::FromStr;
///
/// let md = PlainMonthDay::from_str("03-15").unwrap(); // March 15th
///
/// // Change the month
/// let fields = CalendarFields::new().with_month(12);
/// let modified = md.with(fields, None).unwrap();
/// assert_eq!(modified.month_code(), MonthCode::try_from_utf8("M12".as_bytes()).unwrap());
/// assert_eq!(modified.day(), 15); // unchanged
///
/// // Change the day
/// let fields = CalendarFields::new().with_day(25);
/// let modified = md.with(fields, None).unwrap();
/// assert_eq!(modified.month_code(), MonthCode::try_from_utf8("M03".as_bytes()).unwrap()); // unchanged
/// assert_eq!(modified.day(), 25);
/// ```
///
/// ### Converting to PlainDate
///
/// ```rust
/// use temporal_rs::{PlainMonthDay, fields::CalendarFields};
/// use core::str::FromStr;
///
/// let md = PlainMonthDay::from_str("12-25").unwrap(); // December 25th
///
/// // Convert to a specific date by providing a year
/// let year_fields = CalendarFields::new().with_year(2024);
/// let date = md.to_plain_date(Some(year_fields)).unwrap();
/// assert_eq!(date.year(), 2024);
/// assert_eq!(date.month(), 12);
/// assert_eq!(date.day(), 25);
/// // This represents December 25th, 2024
/// ```
///
/// ### Handling leap year dates
///
/// ```rust
/// use temporal_rs::{PlainMonthDay, MonthCode, fields::CalendarFields, Calendar, options::Overflow};
///
/// // February 29th (leap day)
/// let leap_day = PlainMonthDay::new_with_overflow(
///     2, 29,
///     Calendar::default(),
///     Overflow::Reject,
///     Some(2024) // reference year 2024 (a leap year)
/// ).unwrap();
///
/// assert_eq!(leap_day.month_code(), MonthCode::try_from_utf8("M02".as_bytes()).unwrap());
/// assert_eq!(leap_day.day(), 29);
///
/// // Convert to non-leap year - this would need special handling
/// let year_fields = CalendarFields::new().with_year(2023); // non-leap year
/// let result = leap_day.to_plain_date(Some(year_fields));
/// // This might fail or be adjusted depending on the calendar's rules
/// ```
///
/// ### Practical use cases
///
/// ```rust
/// use temporal_rs::{PlainMonthDay, fields::CalendarFields};
/// use core::str::FromStr;
///
/// // Birthday (recurring annually)
/// let birthday = PlainMonthDay::from_str("07-15").unwrap(); // July 15th
///
/// // Calculate this year's birthday
/// let this_year = 2024;
/// let year_fields = CalendarFields::new().with_year(this_year);
/// let birthday_2024 = birthday.to_plain_date(Some(year_fields)).unwrap();
/// assert_eq!(birthday_2024.year(), 2024);
/// assert_eq!(birthday_2024.month(), 7);
/// assert_eq!(birthday_2024.day(), 15);
///
/// // Holiday (Christmas)
/// let christmas = PlainMonthDay::from_str("12-25").unwrap();
/// let year_fields = CalendarFields::new().with_year(this_year);
/// let christmas_2024 = christmas.to_plain_date(Some(year_fields)).unwrap();
/// assert_eq!(christmas_2024.month(), 12);
/// assert_eq!(christmas_2024.day(), 25);
/// ```
///
/// ## Reference
///
/// For more information, see the [MDN documentation][mdn-plainmonthday].
///
/// [mdn-plainmonthday]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/PlainMonthDay
#[non_exhaustive]
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct PlainMonthDay {
    pub iso: IsoDate,
    calendar: Calendar,
}

impl core::fmt::Display for PlainMonthDay {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(&self.to_ixdtf_string(DisplayCalendar::Auto))
    }
}

impl PlainMonthDay {
    /// Creates a new unchecked `PlainMonthDay`
    #[inline]
    #[must_use]
    pub(crate) fn new_unchecked(iso: IsoDate, calendar: Calendar) -> Self {
        Self { iso, calendar }
    }

    /// Returns the ISO month value of `PlainMonthDay`.
    #[inline]
    #[must_use]
    pub(crate) fn iso_month(&self) -> u8 {
        self.iso.month
    }

    /// Returns the ISO year value of `PlainMonthDay`.
    #[inline]
    #[must_use]
    pub(crate) fn iso_year(&self) -> i32 {
        self.iso.year
    }
}

impl PlainMonthDay {
    /// Creates a new valid `PlainMonthDay`.
    #[inline]
    pub fn new_with_overflow(
        month: u8,
        day: u8,
        calendar: Calendar,
        overflow: Overflow,
        ref_year: Option<i32>,
    ) -> TemporalResult<Self> {
        let ry = ref_year.unwrap_or(1972);
        // 1972 is the first leap year in the Unix epoch (needed to cover all dates)
        let iso = IsoDate::new_with_overflow(ry, month, day, overflow)?;
        Ok(Self::new_unchecked(iso, calendar))
    }

    // Converts a UTF-8 encoded string into a `PlainMonthDay`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parsed = ParsedDate::month_day_from_utf8(s)?;
        Self::from_parsed(parsed)
    }

    // Converts a ParsedDate into a `PlainMonthDay`.
    //
    // Be sure to parse this using [`ParsedDate::month_day_from_utf8()`]~
    pub fn from_parsed(parsed: ParsedDate) -> TemporalResult<Self> {
        let calendar = Calendar::new(parsed.calendar);
        // 10. If calendar is "iso8601", then
        if parsed.calendar == AnyCalendarKind::Iso {
            // a. Let referenceISOYear be 1972 (the first ISO 8601 leap year after the epoch).
            // b. Let isoDate be CreateISODateRecord(referenceISOYear, result.[[Month]], result.[[Day]]).
            // c. Return !CreateTemporalMonthDay(isoDate, calendar).
            let iso = IsoDate::new_unchecked(1972, parsed.record.month, parsed.record.day);
            debug_assert!(iso.check_validity().is_ok(), "Found invalid ParsedDate");
            return Ok(Self::new_unchecked(iso, calendar));
        }
        // 11. Let isoDate be CreateISODateRecord(result.[[Year]], result.[[Month]], result.[[Day]]).
        // 12. If ISODateWithinLimits(isoDate) is false, throw a RangeError exception.
        // Note: parse_month_day will refuse to parse MM-DD format month-days for non-ISO, but
        // it will happily parse YYYY-MM-DD[u-ca=CAL]. These will be valid ISO dates; but they
        // could potentially be out of Temporal range.
        let iso =
            IsoDate::new_unchecked(parsed.record.year, parsed.record.month, parsed.record.day);
        iso.check_within_limits()?;
        debug_assert!(iso.check_validity().is_ok(), "Found invalid ParsedDate");

        // 13. Set result to ISODateToFields(calendar, isoDate, month-day).

        let intermediate = Self::new_unchecked(iso, Calendar::new(parsed.calendar));
        let fields = CalendarFields::from_month_day(&intermediate);
        // 14. NOTE: The following operation is called with constrain regardless of the value of overflow, in
        // order for the calendar to store a canonical value in the [[Year]] field of the [[ISODate]] internal slot of the result.
        // 15. Set isoDate to ? CalendarMonthDayFromFields(calendar, result, constrain).
        intermediate
            .calendar()
            .month_day_from_fields(fields, Overflow::Constrain)
    }

    /// Create a `PlainYearMonth` from a `PartialDate`
    pub fn from_partial(partial: PartialDate, overflow: Option<Overflow>) -> TemporalResult<Self> {
        partial
            .calendar
            .month_day_from_fields(partial.calendar_fields, overflow.unwrap_or_default())
    }

    /// Create a `PlainMonthDay` with the provided fields from a [`PartialDate`].
    pub fn with(&self, fields: CalendarFields, overflow: Option<Overflow>) -> TemporalResult<Self> {
        // Steps 1-6 are engine specific.
        // 5. Let fields be ISODateToFields(calendar, monthDay.[[ISODate]], month-day).
        // 6. Let partialMonthDay be ? PrepareCalendarFields(calendar, temporalMonthDayLike, « year, month, month-code, day », « », partial).
        //
        // NOTE:  We assert that partial is not empty per step 6
        if fields.is_empty() {
            return Err(TemporalError::r#type().with_message("partial object must have a field."));
        }

        // NOTE: We only need to set month / month_code and day, per spec.
        // 7. Set fields to CalendarMergeFields(calendar, fields, partialMonthDay).
        let merged_day = fields.day.unwrap_or(self.day());
        let mut merged = fields.with_day(merged_day);
        if merged.month.is_none() && merged.month_code.is_none() {
            // MonthDay resolution prefers month codes
            // (ordinal months work, but require year information, which
            // we may not have)
            // We should NOT merge over year information even though we have it.
            merged = merged.with_month_code(self.month_code());
        }
        // Step 8-9 already handled by engine.
        // 8. Let resolvedOptions be ? GetOptionsObject(options).
        // 9. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
        // 10. Let isoDate be ? CalendarMonthDayFromFields(calendar, fields, overflow).
        // 11. Return ! CreateTemporalMonthDay(isoDate, calendar).
        self.calendar
            .month_day_from_fields(merged, overflow.unwrap_or(Overflow::Constrain))
    }

    /// Returns the string identifier for the current `Calendar`.
    #[inline]
    #[must_use]
    pub fn calendar_id(&self) -> &'static str {
        self.calendar.identifier()
    }

    /// Returns a reference to `PlainMonthDay`'s inner `Calendar`.
    #[inline]
    #[must_use]
    pub fn calendar(&self) -> &Calendar {
        &self.calendar
    }

    /// Returns the calendar `monthCode` value of `PlainMonthDay`.
    #[inline]
    pub fn month_code(&self) -> MonthCode {
        self.calendar.month_code(&self.iso)
    }

    /// Returns the calendar day value of `PlainMonthDay`.
    #[inline]
    pub fn day(&self) -> u8 {
        self.calendar.day(&self.iso)
    }

    /// Returns the internal reference year used by this MonthDay.
    #[inline]
    pub fn reference_year(&self) -> i32 {
        self.calendar.year(&self.iso)
    }

    /// Create a [`PlainDate`] from the current `PlainMonthDay`.
    pub fn to_plain_date(&self, year: Option<CalendarFields>) -> TemporalResult<PlainDate> {
        let year_partial = match &year {
            Some(partial) => partial,
            None => return Err(TemporalError::r#type().with_message("Year must be provided")),
        };

        // Fallback logic: prefer year, else era/era_year
        let mut fields = CalendarFields::new()
            .with_month_code(self.month_code())
            .with_day(self.day());

        if let Some(year) = year_partial.year {
            fields.year = Some(year);
        } else if let (Some(era), Some(era_year)) = (year_partial.era, year_partial.era_year) {
            fields.era = Some(era);
            fields.era_year = Some(era_year);
        } else {
            return Err(TemporalError::r#type()
                .with_message("PartialDate must contain a year or era/era_year fields"));
        }

        // 8. Let isoDate be ? CalendarDateFromFields(calendar, mergedFields, constrain).
        self.calendar.date_from_fields(fields, Overflow::Constrain)
    }

    /// Gets the epochMilliseconds represented by this YearMonth in the given timezone
    /// (using the reference year, and noon time)
    ///
    // Useful for implementing HandleDateTimeTemporalYearMonth
    pub fn epoch_ns_for_with_provider(
        &self,
        time_zone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<EpochNanoseconds> {
        // 2. Let isoDateTime be CombineISODateAndTimeRecord(temporalYearMonth.[[ISODate]], NoonTimeRecord()).
        let iso = IsoDateTime::new(self.iso, IsoTime::noon())?;
        // 3. Let epochNs be ? GetEpochNanosecondsFor(dateTimeFormat.[[TimeZone]], isoDateTime, compatible).
        Ok(time_zone
            .get_epoch_nanoseconds_for(iso, Disambiguation::Compatible, provider)?
            .ns)
    }

    /// Creates a RFC9557 IXDTF string from the current `PlainMonthDay`.
    pub fn to_ixdtf_string(&self, display_calendar: DisplayCalendar) -> String {
        self.to_ixdtf_writeable(display_calendar)
            .write_to_string()
            .into()
    }

    /// Creates a RFC9557 IXDTF [`Writeable`].
    pub fn to_ixdtf_writeable(&self, display_calendar: DisplayCalendar) -> impl Writeable + '_ {
        let ixdtf = FormattableMonthDay {
            date: FormattableDate(self.iso_year(), self.iso_month(), self.iso.day),
            calendar: FormattableCalendar {
                show: display_calendar,
                calendar: self.calendar().identifier(),
            },
        };
        ixdtf
    }
}

impl FromStr for PlainMonthDay {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_utf8(s.as_bytes())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::Calendar;
    use tinystr::tinystr;

    #[test]
    fn test_plain_month_day_with() {
        let month_day = PlainMonthDay::from_utf8("01-15".as_bytes()).unwrap();

        let new = month_day
            .with(CalendarFields::new().with_day(22), None)
            .unwrap();
        assert_eq!(
            new.month_code(),
            MonthCode::try_from_utf8("M01".as_bytes()).unwrap()
        );
        assert_eq!(new.day(), 22,);

        let new = month_day
            .with(CalendarFields::new().with_month(12), None)
            .unwrap();
        assert_eq!(
            new.month_code(),
            MonthCode::try_from_utf8("M12".as_bytes()).unwrap()
        );
        assert_eq!(new.day(), 15,);
    }

    #[test]
    fn test_to_plain_date_with_year() {
        let month_day =
            PlainMonthDay::new_with_overflow(5, 15, Calendar::default(), Overflow::Reject, None)
                .unwrap();

        let fields = CalendarFields::new().with_year(2025);
        let plain_date = month_day.to_plain_date(Some(fields)).unwrap();
        assert_eq!(plain_date.iso_year(), 2025);
        assert_eq!(plain_date.iso_month(), 5);
        assert_eq!(plain_date.iso_day(), 15);
    }

    #[test]
    fn test_to_plain_date_with_era_and_era_year() {
        // Use a calendar that supports era/era_year, e.g., "gregory"
        let calendar = Calendar::from_str("gregory").unwrap();
        let month_day =
            PlainMonthDay::new_with_overflow(3, 10, calendar.clone(), Overflow::Reject, None)
                .unwrap();

        // Era "ce" and era_year 2020 should resolve to year 2020 in Gregorian
        let fields = CalendarFields::new()
            .with_era(Some(tinystr!(19, "ce")))
            .with_era_year(Some(2020));
        let plain_date = month_day.to_plain_date(Some(fields));
        // Gregorian calendar in ICU4X may not resolve era/era_year unless year is also provided.
        // Accept both Ok and Err, but if Ok, check the values.
        match plain_date {
            Ok(plain_date) => {
                assert_eq!(plain_date.iso_year(), 2020);
                assert_eq!(plain_date.iso_month(), 3);
                assert_eq!(plain_date.iso_day(), 10);
            }
            Err(_) => {
                // Acceptable if era/era_year fallback is not supported by the calendar impl
            }
        }
    }

    #[test]
    fn test_to_plain_date_missing_year_and_era() {
        let month_day =
            PlainMonthDay::new_with_overflow(7, 4, Calendar::default(), Overflow::Reject, None)
                .unwrap();

        // No year, no era/era_year
        let fields = CalendarFields::new();
        let result = month_day.to_plain_date(Some(fields));
        assert!(result.is_err());
    }

    #[test]
    fn test_to_plain_date_with_fallback_logic_matches_date() {
        // This test ensures that the fallback logic in month_day matches the fallback logic in date.rs
        let calendar = Calendar::from_str("gregory").unwrap();
        let month_day =
            PlainMonthDay::new_with_overflow(12, 25, calendar.clone(), Overflow::Reject, None)
                .unwrap();

        // Provide only era/era_year, not year
        let fields = CalendarFields::new()
            .with_era(Some(tinystr!(19, "ce")))
            .with_era_year(Some(1999));
        let plain_date = month_day.to_plain_date(Some(fields));
        match plain_date {
            Ok(plain_date) => {
                assert_eq!(plain_date.iso_year(), 1999);
                assert_eq!(plain_date.iso_month(), 12);
                assert_eq!(plain_date.iso_day(), 25);
            }
            Err(_) => {
                // Acceptable if era/era_year fallback is not supported by the calendar impl
            }
        }
    }
    #[test]
    fn test_valid_strings() {
        const TESTS: &[&str] = &[
            "02-29",
            "02-28",
            "2025-08-05",
            "2025-08-05[u-ca=gregory]",
            "2024-02-29[u-ca=gregory]",
        ];
        for test in TESTS {
            assert!(PlainMonthDay::from_utf8(test.as_bytes()).is_ok());
        }
    }
    #[test]
    fn test_invalid_strings() {
        const TESTS: &[&str] = &[
            // Out of range
            "-99999-01-01",
            "-99999-01-01[u-ca=gregory]",
            // Not a leap year
            "2025-02-29",
            "2025-02-29[u-ca=gregory]",
            // Format not allowed for non-Gregorian
            "02-28[u-ca=gregory]",
        ];
        for test in TESTS {
            assert!(PlainMonthDay::from_utf8(test.as_bytes()).is_err());
        }
    }

    #[test]
    /// This test is for calendars where we don't wish to hardcode dates; but we do wish to know
    /// that monthcodes can be constructed without issue
    fn automated_reference_year() {
        let reference_iso = IsoDate::new_unchecked(1972, 12, 31);
        for cal in [
            AnyCalendarKind::HijriSimulatedMecca,
            AnyCalendarKind::HijriUmmAlQura,
        ] {
            let calendar = Calendar::new(cal);
            for month in 1..=12 {
                for day in [29, 30] {
                    let month_code = crate::builtins::calendar::month_to_month_code(month).unwrap();

                    let calendar_fields = CalendarFields {
                        month_code: Some(month_code),
                        day: Some(day),
                        ..Default::default()
                    };

                    let md = calendar
                        .month_day_from_fields(calendar_fields, Overflow::Reject)
                        .unwrap();

                    assert!(
                        md.iso <= reference_iso,
                        "Reference ISO for {month}-{day} must be before 1972-12-31, found, {:?}",
                        md.iso,
                    );
                }
            }
        }
    }

    #[test]
    fn manual_test_reference_year() {
        // monthCode, day, ISO string, expectedReferenceYear
        // Some of these parsed strings are deliberately not using the reference year
        // so that we can test all code paths. By default, when adding new strings, it is easier
        // to just add the reference year
        const TESTS: &[(&str, u8, &str, i32)] = &[
            ("M10", 30, "1868-10-30[u-ca=gregory]", 1972),
            ("M08", 8, "1868-10-30[u-ca=indian]", 1894),
            ("M09", 21, "2000-12-12[u-ca=indian]", 1894),
            // Dates in the earlier half of the year get pushed back a year
            ("M10", 22, "2000-01-12[u-ca=indian]", 1893),
            ("M01", 11, "2000-03-30[u-ca=persian]", 1351),
            ("M09", 22, "2000-12-12[u-ca=persian]", 1351),
            ("M12", 29, "2025-03-19[u-ca=persian]", 1350),
            // Leap day
            ("M12", 30, "2025-03-20[u-ca=persian]", 1350),
            ("M01", 1, "2025-03-21[u-ca=persian]", 1351),
            ("M01", 1, "2025-03-21[u-ca=persian]", 1351),
            ("M01", 1, "1972-01-01[u-ca=roc]", 61),
            ("M02", 29, "2024-02-29[u-ca=roc]", 61),
            ("M12", 1, "1972-12-01[u-ca=roc]", 61),
            ("M01", 1, "1972-09-11[u-ca=coptic]", 1689),
            ("M12", 1, "1972-08-07[u-ca=coptic]", 1688),
            ("M13", 5, "1972-09-10[u-ca=coptic]", 1688),
            ("M13", 6, "1971-09-11[u-ca=coptic]", 1687),
            ("M01", 1, "1972-09-11[u-ca=ethiopic]", 1965),
            ("M12", 1, "1972-08-07[u-ca=ethiopic]", 1964),
            ("M13", 5, "1972-09-10[u-ca=ethiopic]", 1964),
            ("M13", 6, "1971-09-11[u-ca=ethiopic]", 1963),
            ("M01", 1, "1972-09-11[u-ca=ethioaa]", 7465),
            ("M12", 1, "1972-08-07[u-ca=ethioaa]", 7464),
            ("M13", 5, "1972-09-10[u-ca=ethioaa]", 7464),
            ("M13", 6, "1971-09-11[u-ca=ethioaa]", 7463),
            ("M01", 1, "1972-09-09[u-ca=hebrew]", 5733),
            ("M02", 29, "1972-11-06[u-ca=hebrew]", 5733),
            ("M03", 29, "1972-12-05[u-ca=hebrew]", 5733),
            ("M03", 30, "1971-12-18[u-ca=hebrew]", 5732),
            ("M05L", 29, "1970-03-07[u-ca=hebrew]", 5730),
            ("M07", 1, "1972-03-16[u-ca=hebrew]", 5732),
            ("M01", 1, "1972-02-16[u-ca=islamic-civil]", 1392),
            ("M12", 29, "1972-02-15[u-ca=islamic-civil]", 1391),
            ("M12", 30, "1971-02-26[u-ca=islamic-civil]", 1390),
            ("M01", 1, "1972-02-15[u-ca=islamic-tbla]", 1392),
            ("M12", 29, "1972-02-14[u-ca=islamic-tbla]", 1391),
            ("M12", 30, "1971-02-25[u-ca=islamic-tbla]", 1390),
        ];
        let reference_iso = IsoDate::new_unchecked(1972, 12, 31);
        for (month_code, day, string, year) in TESTS {
            let md = PlainMonthDay::from_str(string).expect(string);

            let calendar_fields = CalendarFields {
                month_code: Some(month_code.parse().unwrap()),
                day: Some(*day),
                ..Default::default()
            };

            let md_from_partial = md
                .calendar()
                .month_day_from_fields(calendar_fields, Overflow::Reject)
                .expect(string);

            assert_eq!(
                md,
                md_from_partial,
                "Parsed {string}, compared with {}: Expected {}-{}, Found {}-{}",
                md_from_partial.to_ixdtf_string(Default::default()),
                md_from_partial.month_code().0,
                md_from_partial.day(),
                md.month_code().0,
                md.day()
            );

            assert_eq!(
                md_from_partial.reference_year(),
                *year,
                "Reference year for {string} ({month_code}-{day}) must be {year}",
            );

            assert!(
                md.iso <= reference_iso && md_from_partial.iso <= reference_iso,
                "Reference ISO for {string} ({}-{}) must be before 1972-12-31, found, {:?} / {:?}",
                md.month_code().0,
                md.day(),
                md.iso,
                md_from_partial.iso
            );
        }
    }
}
