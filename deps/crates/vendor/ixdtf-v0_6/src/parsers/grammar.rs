// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! ISO8601 specific grammar checks.

/// Checks if ascii char is a `AKeyLeadingChar`.
#[inline]
pub(crate) const fn is_a_key_leading_char(ch: u8) -> bool {
    ch.is_ascii_lowercase() || ch == b'_'
}

/// Checks if ascii char is an `AKeyChar`.
#[inline]
pub(crate) const fn is_a_key_char(ch: u8) -> bool {
    is_a_key_leading_char(ch) || ch.is_ascii_digit() || ch == b'-'
}

/// Checks if ascii char is an `AnnotationValueComponent`.
pub(crate) const fn is_annotation_value_component(ch: u8) -> bool {
    ch.is_ascii_digit() || ch.is_ascii_alphabetic()
}

/// Checks if ascii char is a `TZLeadingChar`.
#[inline]
pub(crate) const fn is_tz_leading_char(ch: u8) -> bool {
    ch.is_ascii_alphabetic() || ch == b'_' || ch == b'.'
}

/// Checks if ascii char is a `TZChar`.
#[inline]
pub(crate) const fn is_tz_char(ch: u8) -> bool {
    is_tz_leading_char(ch) || ch.is_ascii_digit() || ch == b'-' || ch == b'+'
}

/// Checks if ascii char is a `TimeZoneIANAName` Separator.
#[inline]
pub(crate) const fn is_tz_name_separator(ch: u8) -> bool {
    ch == b'/'
}

/// Checks if ascii char is an ascii sign.
#[inline]
pub(crate) const fn is_ascii_sign(ch: u8) -> bool {
    ch == b'+' || ch == b'-'
}

/// Checks if ascii char is a `TimeSeparator`.
#[inline]
pub(crate) const fn is_time_separator(ch: u8) -> bool {
    ch == b':'
}

/// Checks if ascii char is a `TimeDesignator`.
#[inline]
pub(crate) const fn is_time_designator(ch: u8) -> bool {
    ch == b'T' || ch == b't'
}

#[inline]
/// Checks if ascii char is a space.
pub(crate) const fn is_space(ch: u8) -> bool {
    ch == b' '
}

/// Checks if ascii char is a `DateTimeSeparator`.
#[inline]
pub(crate) const fn is_date_time_separator(ch: u8) -> bool {
    is_time_designator(ch) || is_space(ch)
}

/// Checks if ascii char is a `UtcDesignator`.
#[inline]
pub(crate) const fn is_utc_designator(ch: u8) -> bool {
    ch == b'Z' || ch == b'z'
}

/// Checks if ascii char is a `DurationDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_duration_designator(ch: u8) -> bool {
    ch == b'P' || ch == b'p'
}

/// Checks if ascii char is a `YearDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_year_designator(ch: u8) -> bool {
    ch == b'Y' || ch == b'y'
}

/// Checks if ascii char is a `MonthsDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_month_designator(ch: u8) -> bool {
    ch == b'M' || ch == b'm'
}

/// Checks if ascii char is a `WeekDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_week_designator(ch: u8) -> bool {
    ch == b'W' || ch == b'w'
}

/// Checks if ascii char is a `DayDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_day_designator(ch: u8) -> bool {
    ch == b'D' || ch == b'd'
}

/// checks if ascii char is a `DayDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_hour_designator(ch: u8) -> bool {
    ch == b'H' || ch == b'h'
}

/// Checks if ascii char is a `MinuteDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_minute_designator(ch: u8) -> bool {
    is_month_designator(ch)
}

/// checks if ascii char is a `SecondDesignator`.
#[inline]
#[cfg(feature = "duration")]
pub(crate) const fn is_second_designator(ch: u8) -> bool {
    ch == b'S' || ch == b's'
}

/// Checks if ascii char is a `DecimalSeparator`.
#[inline]
pub(crate) const fn is_decimal_separator(ch: u8) -> bool {
    ch == b'.' || ch == b','
}

/// Checks if ascii char is an `AnnotationOpen`.
#[inline]
pub(crate) const fn is_annotation_open(ch: u8) -> bool {
    ch == b'['
}

/// Checks if ascii char is an `AnnotationClose`.
#[inline]
pub(crate) const fn is_annotation_close(ch: u8) -> bool {
    ch == b']'
}

/// Checks if ascii char is an `CriticalFlag`.
#[inline]
pub(crate) const fn is_critical_flag(ch: u8) -> bool {
    ch == b'!'
}

/// Checks if ascii char is the `AnnotationKeyValueSeparator`.
#[inline]
pub(crate) const fn is_annotation_key_value_separator(ch: u8) -> bool {
    ch == b'='
}

/// Checks if ascii char is a hyphen. Hyphens are used as a Date separator
/// and as a `AttributeValueComponent` separator.
#[inline]
pub(crate) const fn is_hyphen(ch: u8) -> bool {
    ch == b'-'
}
