//! This module implements Temporal Date/Time parsing functionality.

use crate::{
    iso::{IsoDate, IsoTime},
    options::{DisplayCalendar, DisplayOffset, DisplayTimeZone},
    Sign, TemporalError, TemporalResult,
};
use ixdtf::{
    encoding::Utf8,
    parsers::IxdtfParser,
    records::{Annotation, DateRecord, IxdtfParseRecord, TimeRecord, UtcOffsetRecordOrZ},
};
use writeable::{impl_display_with_writeable, LengthHint, Writeable};

mod time_zone;

pub(crate) use time_zone::{parse_allowed_timezone_formats, parse_identifier};

// TODO: Move `Writeable` functionality to `ixdtf` crate

#[derive(Debug, Default)]
pub struct IxdtfStringBuilder<'a> {
    inner: FormattableIxdtf<'a>,
}

impl<'a> IxdtfStringBuilder<'a> {
    pub fn with_date(mut self, iso: IsoDate) -> Self {
        self.inner.date = Some(FormattableDate(iso.year, iso.month, iso.day));
        self
    }

    pub fn with_time(mut self, time: IsoTime, precision: Precision) -> Self {
        let nanosecond = (time.millisecond as u32 * 1_000_000)
            + (time.microsecond as u32 * 1000)
            + time.nanosecond as u32;

        self.inner.time = Some(FormattableTime {
            hour: time.hour,
            minute: time.minute,
            second: time.second,
            nanosecond,
            precision,
            include_sep: true,
        });
        self
    }

    pub fn with_minute_offset(
        mut self,
        sign: Sign,
        hour: u8,
        minute: u8,
        show: DisplayOffset,
    ) -> Self {
        let time = FormattableTime {
            hour,
            minute,
            second: 9,
            nanosecond: 0,
            precision: Precision::Minute,
            include_sep: true,
        };

        self.inner.utc_offset = Some(FormattableUtcOffset {
            show,
            offset: UtcOffset::Offset(FormattableOffset { sign, time }),
        });
        self
    }

    pub fn with_z(mut self, show: DisplayOffset) -> Self {
        self.inner.utc_offset = Some(FormattableUtcOffset {
            show,
            offset: UtcOffset::Z,
        });
        self
    }

    pub fn with_timezone(mut self, timezone: &'a str, show: DisplayTimeZone) -> Self {
        self.inner.timezone = Some(FormattableTimeZone { show, timezone });
        self
    }

    pub fn with_calendar(mut self, calendar: &'static str, show: DisplayCalendar) -> Self {
        self.inner.calendar = Some(FormattableCalendar { show, calendar });
        self
    }

    pub fn build(self) -> alloc::string::String {
        self.inner.to_string()
    }
}

impl Writeable for IxdtfStringBuilder<'_> {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        self.inner.write_to(sink)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        self.inner.writeable_length_hint()
    }
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Precision {
    #[default]
    Auto,
    Minute,
    Digit(u8),
}

#[derive(Debug)]
pub struct FormattableTime {
    pub hour: u8,
    pub minute: u8,
    pub second: u8,
    pub nanosecond: u32,
    pub precision: Precision,
    pub include_sep: bool,
}

impl Writeable for FormattableTime {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        write_padded_u8(self.hour, sink)?;
        if self.include_sep {
            sink.write_char(':')?;
        }
        write_padded_u8(self.minute, sink)?;
        if self.precision == Precision::Minute {
            return Ok(());
        }
        if self.include_sep {
            sink.write_char(':')?;
        }
        write_padded_u8(self.second, sink)?;
        if (self.nanosecond == 0 && self.precision == Precision::Auto)
            || self.precision == Precision::Digit(0)
        {
            return Ok(());
        }
        sink.write_char('.')?;
        write_nanosecond(self.nanosecond, self.precision, sink)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        let sep = self.include_sep as usize;
        if self.precision == Precision::Minute {
            return LengthHint::exact(4 + sep);
        }
        let time_base = 6 + (sep * 2);
        if self.nanosecond == 0 || self.precision == Precision::Digit(0) {
            return LengthHint::exact(time_base);
        }
        if let Precision::Digit(d) = self.precision {
            return LengthHint::exact(time_base + 1 + d as usize);
        }
        LengthHint::between(time_base + 2, time_base + 10)
    }
}

#[derive(Debug)]
pub struct FormattableUtcOffset {
    pub show: DisplayOffset,
    pub offset: UtcOffset,
}

#[derive(Debug)]
pub enum UtcOffset {
    Z,
    Offset(FormattableOffset),
}

impl Writeable for FormattableUtcOffset {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        if self.show == DisplayOffset::Never {
            return Ok(());
        }
        match &self.offset {
            UtcOffset::Z => sink.write_char('Z'),
            UtcOffset::Offset(offset) => offset.write_to(sink),
        }
    }

    fn writeable_length_hint(&self) -> LengthHint {
        match &self.offset {
            UtcOffset::Z => LengthHint::exact(1),
            UtcOffset::Offset(o) => o.writeable_length_hint(),
        }
    }
}

fn write_padded_u8<W: core::fmt::Write + ?Sized>(num: u8, sink: &mut W) -> core::fmt::Result {
    if num < 10 {
        sink.write_char('0')?;
    }
    num.write_to(sink)
}

fn write_nanosecond<W: core::fmt::Write + ?Sized>(
    nanoseconds: u32,
    precision: Precision,
    sink: &mut W,
) -> core::fmt::Result {
    let (digits, index) = u32_to_digits(nanoseconds);
    let precision = match precision {
        Precision::Digit(digit) if digit <= 9 => digit as usize,
        _ => index,
    };
    write_digit_slice_to_precision(digits, 0, precision, sink)
}

pub fn u32_to_digits(mut value: u32) -> ([u8; 9], usize) {
    let mut output = [0; 9];
    let mut precision = 0;
    for (i, out) in output.iter_mut().enumerate().rev() {
        let v = (value % 10) as u8;
        value /= 10;
        if precision == 0 && v != 0 {
            // i is 0-indexed, but we want a 1-indexed precision
            precision = i + 1;
        }
        *out = v;
    }

    (output, precision)
}

pub fn write_digit_slice_to_precision<W: core::fmt::Write + ?Sized>(
    digits: [u8; 9],
    base: usize,
    precision: usize,
    sink: &mut W,
) -> core::fmt::Result {
    for digit in digits.iter().take(precision).skip(base) {
        digit.write_to(sink)?;
    }
    Ok(())
}

#[derive(Debug)]
pub struct FormattableOffset {
    pub sign: Sign,
    pub time: FormattableTime,
}

impl Writeable for FormattableOffset {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        match self.sign {
            Sign::Negative => sink.write_char('-')?,
            _ => sink.write_char('+')?,
        }
        self.time.write_to(sink)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        self.time.writeable_length_hint() + 1
    }
}

impl_display_with_writeable!(FormattableIxdtf<'_>);
impl_display_with_writeable!(FormattableMonthDay<'_>);
impl_display_with_writeable!(FormattableYearMonth<'_>);
impl_display_with_writeable!(FormattableDuration);
impl_display_with_writeable!(FormattableDate);
impl_display_with_writeable!(FormattableTime);
impl_display_with_writeable!(FormattableUtcOffset);
impl_display_with_writeable!(FormattableOffset);
impl_display_with_writeable!(FormattableTimeZone<'_>);
impl_display_with_writeable!(FormattableCalendar<'_>);

#[derive(Debug)]
pub struct FormattableDate(pub i32, pub u8, pub u8);

impl Writeable for FormattableDate {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        write_year(self.0, sink)?;
        sink.write_char('-')?;
        write_padded_u8(self.1, sink)?;
        sink.write_char('-')?;
        write_padded_u8(self.2, sink)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        let year_length = if (0..=9999).contains(&self.0) { 4 } else { 7 };

        LengthHint::exact(6 + year_length)
    }
}

fn write_year<W: core::fmt::Write + ?Sized>(year: i32, sink: &mut W) -> core::fmt::Result {
    if (0..=9999).contains(&year) {
        write_four_digit_year(year, sink)
    } else {
        write_extended_year(year, sink)
    }
}

fn write_four_digit_year<W: core::fmt::Write + ?Sized>(
    mut y: i32,
    sink: &mut W,
) -> core::fmt::Result {
    (y / 1_000).write_to(sink)?;
    y %= 1_000;
    (y / 100).write_to(sink)?;
    y %= 100;
    (y / 10).write_to(sink)?;
    y %= 10;
    y.write_to(sink)
}

fn write_extended_year<W: core::fmt::Write + ?Sized>(y: i32, sink: &mut W) -> core::fmt::Result {
    let sign = if y < 0 { '-' } else { '+' };
    sink.write_char(sign)?;
    let (digits, _) = u32_to_digits(y.unsigned_abs());
    // SAFETY: digits slice is made up up valid ASCII digits.
    write_digit_slice_to_precision(digits, 3, 9, sink)
}

#[derive(Debug)]
pub struct FormattableTimeZone<'a> {
    pub show: DisplayTimeZone,
    pub timezone: &'a str,
}

impl Writeable for FormattableTimeZone<'_> {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        if self.show == DisplayTimeZone::Never {
            return Ok(());
        }
        sink.write_char('[')?;
        if self.show == DisplayTimeZone::Critical {
            sink.write_char('!')?;
        }
        sink.write_str(self.timezone)?;
        sink.write_char(']')
    }

    fn writeable_length_hint(&self) -> writeable::LengthHint {
        if self.show == DisplayTimeZone::Never {
            return LengthHint::exact(0);
        }
        let critical = (self.show == DisplayTimeZone::Critical) as usize;
        LengthHint::exact(2 + critical + self.timezone.len())
    }
}

#[derive(Debug)]
pub struct FormattableCalendar<'a> {
    pub show: DisplayCalendar,
    pub calendar: &'a str,
}

impl Writeable for FormattableCalendar<'_> {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        if self.show == DisplayCalendar::Never
            || self.show == DisplayCalendar::Auto && self.calendar == "iso8601"
        {
            return Ok(());
        }
        sink.write_char('[')?;
        if self.show == DisplayCalendar::Critical {
            sink.write_char('!')?;
        }
        sink.write_str("u-ca=")?;
        sink.write_str(self.calendar)?;
        sink.write_char(']')
    }

    fn writeable_length_hint(&self) -> LengthHint {
        if self.show == DisplayCalendar::Never
            || self.show == DisplayCalendar::Auto && self.calendar == "iso8601"
        {
            return LengthHint::exact(0);
        }
        let critical = (self.show == DisplayCalendar::Critical) as usize;
        LengthHint::exact(7 + critical + self.calendar.len())
    }
}

#[derive(Debug)]
pub struct FormattableMonthDay<'a> {
    pub date: FormattableDate,
    pub calendar: FormattableCalendar<'a>,
}

impl Writeable for FormattableMonthDay<'_> {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        if self.calendar.show == DisplayCalendar::Always
            || self.calendar.show == DisplayCalendar::Critical
            || self.calendar.calendar != "iso8601"
        {
            write_year(self.date.0, sink)?;
            sink.write_char('-')?;
        }
        write_padded_u8(self.date.1, sink)?;
        sink.write_char('-')?;
        write_padded_u8(self.date.2, sink)?;
        self.calendar.write_to(sink)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        let base_length = self.calendar.writeable_length_hint() + LengthHint::exact(5);
        if self.calendar.show == DisplayCalendar::Always
            || self.calendar.show == DisplayCalendar::Critical
            || self.calendar.calendar != "iso8601"
        {
            let year_length = if (0..=9999).contains(&self.date.0) {
                4
            } else {
                7
            };
            return base_length + LengthHint::exact(year_length);
        }
        base_length
    }
}

#[derive(Debug)]
pub struct FormattableYearMonth<'a> {
    pub date: FormattableDate,
    pub calendar: FormattableCalendar<'a>,
}

impl Writeable for FormattableYearMonth<'_> {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        write_year(self.date.0, sink)?;
        sink.write_char('-')?;
        write_padded_u8(self.date.1, sink)?;
        if self.calendar.show == DisplayCalendar::Always
            || self.calendar.show == DisplayCalendar::Critical
            || self.calendar.calendar != "iso8601"
        {
            sink.write_char('-')?;
            write_padded_u8(self.date.2, sink)?;
        }

        self.calendar.write_to(sink)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        let year_length = if (0..=9999).contains(&self.date.0) {
            4
        } else {
            7
        };
        let base_length =
            self.calendar.writeable_length_hint() + LengthHint::exact(year_length + 3);
        if self.calendar.show == DisplayCalendar::Always
            || self.calendar.show == DisplayCalendar::Critical
            || self.calendar.calendar != "iso8601"
        {
            return base_length + LengthHint::exact(3);
        }
        base_length
    }
}

#[derive(Debug, Default)]
pub struct FormattableIxdtf<'a> {
    pub date: Option<FormattableDate>,
    pub time: Option<FormattableTime>,
    pub utc_offset: Option<FormattableUtcOffset>,
    pub timezone: Option<FormattableTimeZone<'a>>,
    pub calendar: Option<FormattableCalendar<'a>>,
}

impl Writeable for FormattableIxdtf<'_> {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        if let Some(date) = &self.date {
            date.write_to(sink)?;
        }
        if let Some(time) = &self.time {
            if self.date.is_some() {
                sink.write_char('T')?;
            }
            time.write_to(sink)?;
        }
        if let Some(offset) = &self.utc_offset {
            offset.write_to(sink)?;
        }
        if let Some(timezone) = &self.timezone {
            timezone.write_to(sink)?;
        }
        if let Some(calendar) = &self.calendar {
            calendar.write_to(sink)?;
        }

        Ok(())
    }

    fn writeable_length_hint(&self) -> LengthHint {
        let date_length = self
            .date
            .as_ref()
            .map(|d| d.writeable_length_hint())
            .unwrap_or(LengthHint::exact(0));
        let time_length = self
            .time
            .as_ref()
            .map(|t| {
                let t_present = self.date.is_some() as usize;
                t.writeable_length_hint() + t_present
            })
            .unwrap_or(LengthHint::exact(0));
        let utc_length = self
            .utc_offset
            .as_ref()
            .map(|utc| utc.writeable_length_hint())
            .unwrap_or(LengthHint::exact(0));
        let timezone_length = self
            .timezone
            .as_ref()
            .map(|tz| tz.writeable_length_hint())
            .unwrap_or(LengthHint::exact(0));
        let cal_length = self
            .calendar
            .as_ref()
            .map(|cal| cal.writeable_length_hint())
            .unwrap_or(LengthHint::exact(0));

        date_length + time_length + utc_length + timezone_length + cal_length
    }
}

#[derive(Debug, Clone, Copy)]
pub struct FormattableDateDuration {
    pub years: u32,
    pub months: u32,
    pub weeks: u32,
    pub days: u64,
}

#[derive(Debug, Clone, Copy)]
pub enum FormattableTimeDuration {
    Hours(u64, Option<u32>),
    Minutes(u64, u64, Option<u32>),
    Seconds(u64, u64, u64, Option<u32>),
}

pub struct FormattableDuration {
    pub precision: Precision,
    pub sign: Sign,
    pub date: Option<FormattableDateDuration>,
    pub time: Option<FormattableTimeDuration>,
}

impl Writeable for FormattableDuration {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        if self.sign == Sign::Negative {
            sink.write_char('-')?;
        }
        sink.write_char('P')?;
        if let Some(date) = self.date {
            checked_write_u32_with_suffix(date.years, 'Y', sink)?;
            checked_write_u32_with_suffix(date.months, 'M', sink)?;
            checked_write_u32_with_suffix(date.weeks, 'W', sink)?;
            checked_write_u64_with_suffix(date.days, 'D', sink)?;
        }
        if let Some(time) = self.time {
            match time {
                FormattableTimeDuration::Hours(hours, fraction) => {
                    let ns = fraction.unwrap_or(0);
                    if hours + u64::from(ns) != 0 {
                        sink.write_char('T')?;
                    }
                    if hours == 0 {
                        return Ok(());
                    }
                    hours.write_to(sink)?;
                    if ns != 0 {
                        sink.write_char('.')?;
                        ns.write_to(sink)?;
                    }
                    sink.write_char('H')?;
                }
                FormattableTimeDuration::Minutes(hours, minutes, fraction) => {
                    let ns = fraction.unwrap_or(0);
                    if hours + minutes + u64::from(ns) != 0 {
                        sink.write_char('T')?;
                    }
                    checked_write_u64_with_suffix(hours, 'H', sink)?;
                    if minutes == 0 {
                        return Ok(());
                    }
                    minutes.write_to(sink)?;
                    if ns != 0 {
                        sink.write_char('.')?;
                        ns.write_to(sink)?;
                    }
                    sink.write_char('M')?;
                }
                FormattableTimeDuration::Seconds(hours, minutes, seconds, fraction) => {
                    let ns = fraction.unwrap_or(0);
                    let unit_below_minute = self.date.is_none() && hours == 0 && minutes == 0;

                    let write_second = seconds != 0
                        || unit_below_minute
                        || matches!(self.precision, Precision::Digit(_));

                    if hours != 0 || minutes != 0 || write_second {
                        sink.write_char('T')?;
                    }

                    checked_write_u64_with_suffix(hours, 'H', sink)?;
                    checked_write_u64_with_suffix(minutes, 'M', sink)?;
                    if write_second {
                        seconds.write_to(sink)?;
                        if self.precision == Precision::Digit(0)
                            || (self.precision == Precision::Auto && ns == 0)
                        {
                            sink.write_char('S')?;
                            return Ok(());
                        }
                        sink.write_char('.')?;
                        write_nanosecond(ns, self.precision, sink)?;
                        sink.write_char('S')?;
                    }
                }
            }
        }
        Ok(())
    }
}

fn checked_write_u32_with_suffix<W: core::fmt::Write + ?Sized>(
    val: u32,
    suffix: char,
    sink: &mut W,
) -> core::fmt::Result {
    if val == 0 {
        return Ok(());
    }
    val.write_to(sink)?;
    sink.write_char(suffix)
}

fn checked_write_u64_with_suffix<W: core::fmt::Write + ?Sized>(
    val: u64,
    suffix: char,
    sink: &mut W,
) -> core::fmt::Result {
    if val == 0 {
        return Ok(());
    }
    val.write_to(sink)?;
    sink.write_char(suffix)
}

// TODO: Determine if these should be separate structs, i.e. TemporalDateTimeParser/TemporalInstantParser, or
// maybe on global `TemporalParser` around `IxdtfParser` that handles the Temporal idiosyncracies.
#[derive(PartialEq)]
enum ParseVariant {
    YearMonth,
    MonthDay,
    DateTime,
    Time,
}

#[inline]
fn parse_ixdtf(source: &[u8], variant: ParseVariant) -> TemporalResult<IxdtfParseRecord<'_, Utf8>> {
    fn cast_handler<'a>(
        _: &mut IxdtfParser<'a, Utf8>,
        handler: impl FnMut(Annotation<'a, Utf8>) -> Option<Annotation<'a, Utf8>>,
    ) -> impl FnMut(Annotation<'a, Utf8>) -> Option<Annotation<'a, Utf8>> {
        handler
    }

    let mut first_calendar: Option<Annotation<Utf8>> = None;
    let mut critical_duplicate_calendar = false;
    let mut parser = IxdtfParser::from_utf8(source);

    let handler = cast_handler(&mut parser, |annotation: Annotation<Utf8>| {
        if annotation.key == "u-ca".as_bytes() {
            match first_calendar {
                Some(ref cal) => {
                    if cal.critical || annotation.critical {
                        critical_duplicate_calendar = true
                    }
                }
                None => first_calendar = Some(annotation),
            }
            return None;
        }

        // Make the parser handle any unknown annotation.
        Some(annotation)
    });

    let mut record = match variant {
        ParseVariant::YearMonth => parser.parse_year_month_with_annotation_handler(handler),
        ParseVariant::MonthDay => parser.parse_month_day_with_annotation_handler(handler),
        ParseVariant::DateTime => parser.parse_with_annotation_handler(handler),
        ParseVariant::Time => parser.parse_time_with_annotation_handler(handler),
    }?;

    record.calendar = first_calendar.map(|v| v.value);

    // Note: this method only handles the specific AnnotatedFoo nonterminals;
    // so if we are parsing MonthDay/YearMonth we will never have a DateDay/DateYear parse node.
    //
    // 3. If goal is TemporalYearMonthString, and parseResult does not contain a DateDay Parse Node, then
    //  a. If calendar is not empty, and the ASCII-lowercase of calendar is not "iso8601", throw a RangeError exception.
    // 4. If goal is TemporalMonthDayString and parseResult does not contain a DateYear Parse Node, then
    //  a. If calendar is not empty, and the ASCII-lowercase of calendar is not "iso8601", throw a RangeError exception.
    //  b. Set yearAbsent to true.
    if variant == ParseVariant::MonthDay || variant == ParseVariant::YearMonth {
        if let Some(cal) = record.calendar {
            if !cal.eq_ignore_ascii_case(b"iso8601") {
                return Err(TemporalError::range()
                    .with_message("YearMonth/MonthDay formats only allowed for ISO calendar."));
            }
        }
    }

    if critical_duplicate_calendar {
        // TODO: Add tests for the below.
        // Parser handles non-matching calendar, so the value thrown here should only be duplicates.
        return Err(TemporalError::range()
            .with_message("Duplicate calendar value with critical flag found."));
    }

    // Validate that the DateRecord exists.
    if variant != ParseVariant::Time && record.date.is_none() {
        return Err(
            TemporalError::range().with_message("DateTime strings must contain a Date value.")
        );
    }

    // Temporal requires parsed dates to be checked for validity at parse time
    // https://tc39.es/proposal-temporal/#sec-temporal-iso8601grammar-static-semantics-early-errors
    // https://tc39.es/proposal-temporal/#sec-temporal-iso8601grammar-static-semantics-isvaliddate
    // https://tc39.es/proposal-temporal/#sec-temporal-iso8601grammar-static-semantics-isvalidmonthday
    if let Some(date) = record.date {
        // ixdtf currently returns always-valid reference years/days
        // in these cases (year = 0, day = 1), but we should set our own
        // for robustness.
        let year = if variant == ParseVariant::MonthDay {
            1972
        } else {
            date.year
        };
        let day = if variant == ParseVariant::YearMonth {
            1
        } else {
            date.day
        };
        if !crate::iso::is_valid_date(year, date.month, day) {
            return Err(TemporalError::range()
                .with_message("DateTime strings must contain a valid ISO date."));
        }
    }

    Ok(record)
}

/// A utility function for parsing a `DateTime` string
#[inline]
pub(crate) fn parse_date_time(source: &[u8]) -> TemporalResult<IxdtfParseRecord<'_, Utf8>> {
    let record = parse_ixdtf(source, ParseVariant::DateTime)?;

    if record.offset == Some(UtcOffsetRecordOrZ::Z) {
        return Err(TemporalError::range()
            .with_message("UTC designator is not valid for DateTime parsing."));
    }

    Ok(record)
}

#[inline]
pub(crate) fn parse_zoned_date_time(source: &[u8]) -> TemporalResult<IxdtfParseRecord<'_, Utf8>> {
    let record = parse_ixdtf(source, ParseVariant::DateTime)?;

    // TODO: Support rejecting subminute precision in time zone annootations
    if record.tz.is_none() {
        return Err(TemporalError::range()
            .with_message("Time zone annotation is required for parsing a zoned date time."));
    }

    Ok(record)
}

pub(crate) struct IxdtfParseInstantRecord {
    pub(crate) date: DateRecord,
    pub(crate) time: TimeRecord,
    pub(crate) offset: UtcOffsetRecordOrZ,
}

/// A utility function for parsing an `Instant` string
#[inline]
pub(crate) fn parse_instant(source: &[u8]) -> TemporalResult<IxdtfParseInstantRecord> {
    let record = parse_ixdtf(source, ParseVariant::DateTime)?;

    let IxdtfParseRecord {
        date: Some(date),
        time: Some(time),
        offset: Some(offset),
        ..
    } = record
    else {
        return Err(
            TemporalError::range().with_message("Required fields missing from Instant string.")
        );
    };

    Ok(IxdtfParseInstantRecord { date, time, offset })
}

// Ensure that the record does not have an offset element.
//
// This handles the [~Zoned] in TemporalFooString productions
fn check_offset(record: IxdtfParseRecord<'_, Utf8>) -> TemporalResult<IxdtfParseRecord<'_, Utf8>> {
    if record.offset == Some(UtcOffsetRecordOrZ::Z) {
        return Err(TemporalError::range()
            .with_message("UTC designator is not valid for plain date/time parsing."));
    }
    Ok(record)
}

/// A utility function for parsing a `YearMonth` string
#[inline]
pub(crate) fn parse_year_month(source: &[u8]) -> TemporalResult<IxdtfParseRecord<'_, Utf8>> {
    let ym_record = parse_ixdtf(source, ParseVariant::YearMonth);

    let Err(e) = ym_record else {
        return ym_record.and_then(check_offset);
    };

    let dt_parse = parse_date_time(source);

    match dt_parse {
        Ok(dt) => check_offset(dt),
        // Format and return the error from parsing YearMonth.
        _ => Err(e),
    }
}

/// A utilty function for parsing a `MonthDay` String.
pub(crate) fn parse_month_day(source: &[u8]) -> TemporalResult<IxdtfParseRecord<'_, Utf8>> {
    let md_record = parse_ixdtf(source, ParseVariant::MonthDay);
    let Err(e) = md_record else {
        return md_record.and_then(check_offset);
    };

    let dt_parse = parse_date_time(source);

    match dt_parse {
        Ok(dt) => check_offset(dt),
        // Format and return the error from parsing MonthDay.
        _ => Err(e),
    }
}

// Ensures that an IxdtfParseRecord was parsed with [~Zoned][+TimeRequired]
fn check_time_record(record: IxdtfParseRecord<Utf8>) -> TemporalResult<TimeRecord> {
    // Handle [~Zoned]
    let record = check_offset(record)?;
    // Handle [+TimeRequired]
    let Some(time) = record.time else {
        return Err(TemporalError::range()
            .with_message("PlainTime can only be parsed from strings with a time component."));
    };
    Ok(time)
}

#[inline]
pub(crate) fn parse_time(source: &[u8]) -> TemporalResult<TimeRecord> {
    let time_record = parse_ixdtf(source, ParseVariant::Time);

    let Err(e) = time_record else {
        return time_record.and_then(check_time_record);
    };

    let dt_parse = parse_date_time(source);

    match dt_parse {
        Ok(dt) => check_time_record(dt),
        // Format and return the error from parsing MonthDay.
        _ => Err(e),
    }
}

/// Consider this API to be unstable: it is used internally by temporal_capi but
/// will likely be replaced with a proper TemporalParser API at some point.
#[inline]
pub fn parse_allowed_calendar_formats(s: &[u8]) -> Option<&[u8]> {
    if let Ok(r) = parse_ixdtf(s, ParseVariant::DateTime).map(|r| r.calendar) {
        return Some(r.unwrap_or(&[]));
    } else if let Ok(r) = IxdtfParser::from_utf8(s).parse_time().map(|r| r.calendar) {
        return Some(r.unwrap_or(&[]));
    } else if let Ok(r) = parse_ixdtf(s, ParseVariant::YearMonth).map(|r| r.calendar) {
        return Some(r.unwrap_or(&[]));
    } else if let Ok(r) = parse_ixdtf(s, ParseVariant::MonthDay).map(|r| r.calendar) {
        return Some(r.unwrap_or(&[]));
    }
    None
}

// TODO: ParseTimeZoneString, ParseZonedDateTimeString

#[cfg(test)]
mod tests {
    use super::{FormattableDate, FormattableOffset};
    use crate::parsers::{FormattableTime, Precision};
    use alloc::format;
    use writeable::assert_writeable_eq;

    #[test]
    fn offset_string() {
        let offset = FormattableOffset {
            sign: crate::Sign::Positive,
            time: FormattableTime {
                hour: 4,
                minute: 0,
                second: 0,
                nanosecond: 0,
                precision: Precision::Minute,
                include_sep: true,
            },
        };
        assert_writeable_eq!(offset, "+04:00");

        let offset = FormattableOffset {
            sign: crate::Sign::Negative,
            time: FormattableTime {
                hour: 5,
                minute: 0,
                second: 30,
                nanosecond: 0,
                precision: Precision::Minute,
                include_sep: true,
            },
        };
        assert_writeable_eq!(offset, "-05:00");

        let offset = FormattableOffset {
            sign: crate::Sign::Negative,
            time: FormattableTime {
                hour: 5,
                minute: 0,
                second: 30,
                nanosecond: 0,
                precision: Precision::Auto,
                include_sep: true,
            },
        };
        assert_writeable_eq!(offset, "-05:00:30");

        let offset = FormattableOffset {
            sign: crate::Sign::Negative,
            time: FormattableTime {
                hour: 5,
                minute: 0,
                second: 00,
                nanosecond: 123050000,
                precision: Precision::Auto,
                include_sep: true,
            },
        };
        assert_writeable_eq!(offset, "-05:00:00.12305");
    }

    #[test]
    fn time_to_precision() {
        let time = FormattableTime {
            hour: 5,
            minute: 0,
            second: 00,
            nanosecond: 123050000,
            precision: Precision::Digit(8),
            include_sep: true,
        };
        assert_writeable_eq!(time, "05:00:00.12305000");

        let time = FormattableTime {
            hour: 5,
            minute: 0,
            second: 00,
            nanosecond: 123050002,
            precision: Precision::Digit(9),
            include_sep: true,
        };
        assert_writeable_eq!(time, "05:00:00.123050002");

        let time = FormattableTime {
            hour: 5,
            minute: 0,
            second: 00,
            nanosecond: 123050000,
            precision: Precision::Digit(1),
            include_sep: true,
        };
        assert_writeable_eq!(time, "05:00:00.1");

        let time = FormattableTime {
            hour: 5,
            minute: 0,
            second: 00,
            nanosecond: 123050000,
            precision: Precision::Digit(0),
            include_sep: true,
        };
        assert_writeable_eq!(time, "05:00:00");
    }

    #[test]
    fn date_string() {
        let date = FormattableDate(2024, 12, 8);
        assert_writeable_eq!(date, "2024-12-08");

        let date = FormattableDate(987654, 12, 8);
        assert_writeable_eq!(date, "+987654-12-08");

        let date = FormattableDate(-987654, 12, 8);
        assert_writeable_eq!(date, "-987654-12-08");

        let date = FormattableDate(0, 12, 8);
        assert_writeable_eq!(date, "0000-12-08");

        let date = FormattableDate(10_000, 12, 8);
        assert_writeable_eq!(date, "+010000-12-08");

        let date = FormattableDate(-10_000, 12, 8);
        assert_writeable_eq!(date, "-010000-12-08");
    }
}
