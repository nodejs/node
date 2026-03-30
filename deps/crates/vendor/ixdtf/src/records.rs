// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! The records that `ixdtf`'s contain the resulting values of parsing.

use core::num::NonZeroU8;

use crate::core::EncodingType;

/// An `IxdtfParseRecord` is an intermediary record returned by `IxdtfParser`.
#[non_exhaustive]
#[derive(Default, Debug, PartialEq)]
pub struct IxdtfParseRecord<'a, T: EncodingType> {
    /// Parsed `DateRecord`
    pub date: Option<DateRecord>,
    /// Parsed `TimeRecord`
    pub time: Option<TimeRecord>,
    /// Parsed UtcOffset
    pub offset: Option<UtcOffsetRecordOrZ>,
    /// Parsed `TimeZone` annotation with critical flag and data (UTCOffset | IANA name)
    pub tz: Option<TimeZoneAnnotation<'a, T>>,
    /// The parsed calendar value.
    pub calendar: Option<&'a [T::CodeUnit]>,
}

#[non_exhaustive]
#[derive(Debug, Clone, PartialEq)]
/// A record of an annotation.
pub struct Annotation<'a, T: EncodingType> {
    /// Whether this annotation is flagged as critical
    pub critical: bool,
    /// The parsed key value of the annotation
    pub key: &'a [T::CodeUnit],
    /// The parsed value of the annotation
    pub value: &'a [T::CodeUnit],
}

#[allow(clippy::exhaustive_structs)] // DateRecord only allows for a year, month, and day value.
#[derive(Default, Debug, Clone, Copy, PartialEq)]
/// The record of a parsed date.
pub struct DateRecord {
    /// Date Year
    pub year: i32,
    /// Date Month
    pub month: u8,
    /// Date Day
    pub day: u8,
}

/// Parsed Time info
#[allow(clippy::exhaustive_structs)] // TimeRecord only allows for a hour, minute, second, and sub-second value.
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct TimeRecord {
    /// An hour
    pub hour: u8,
    /// A minute value
    pub minute: u8,
    /// A second value.
    pub second: u8,
    /// A nanosecond value representing all sub-second components.
    pub fraction: Option<Fraction>,
}

/// A `TimeZoneAnnotation` that represents a parsed `TimeZoneRecord` and its critical flag.
#[non_exhaustive]
#[derive(Debug, Clone, PartialEq)]
pub struct TimeZoneAnnotation<'a, T: EncodingType> {
    /// Critical flag for the `TimeZoneAnnotation`.
    pub critical: bool,
    /// The parsed `TimeZoneRecord` for the annotation.
    pub tz: TimeZoneRecord<'a, T>,
}

/// Parsed `TimeZone` data, which can be either a UTC Offset value or IANA Time Zone Name value.
#[non_exhaustive]
#[derive(Debug, Clone, PartialEq)]
pub enum TimeZoneRecord<'a, T: EncodingType> {
    /// TimeZoneIANAName
    Name(&'a [T::CodeUnit]),
    /// TimeZoneOffset
    Offset(MinutePrecisionOffset),
}

/// The parsed sign value, representing whether its struct is positive or negative.
#[repr(i8)]
#[allow(clippy::exhaustive_enums)] // Sign can only be positive or negative.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Sign {
    /// A negative value sign, representable as either -1 or false.
    Negative = -1,
    /// A positive value sign, representable as either 1 or true.
    Positive = 1,
}

impl From<bool> for Sign {
    fn from(value: bool) -> Self {
        match value {
            true => Self::Positive,
            false => Self::Negative,
        }
    }
}

/// A `UtcOffsetRecord` that is either a minute precision or
/// full precision UTC offset.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum UtcOffsetRecord {
    // A UTC offset that is only precise to the minute
    MinutePrecision(MinutePrecisionOffset),
    // A UTC offset with full fractional second precision
    FullPrecisionOffset(FullPrecisionOffset),
}

impl UtcOffsetRecord {
    /// Returns whether the UTC offset is a minute precision offset.
    pub fn is_minute_precision(&self) -> bool {
        matches!(self, Self::MinutePrecision(_))
    }

    /// Returrns a zerod UTC Offset in minute precision
    pub fn zero() -> Self {
        Self::MinutePrecision(MinutePrecisionOffset::zero())
    }

    /// Returns the `Sign` of this UTC offset.
    pub fn sign(&self) -> Sign {
        match self {
            Self::MinutePrecision(offset) => offset.sign,
            Self::FullPrecisionOffset(offset) => offset.minute_precision_offset.sign,
        }
    }

    /// Returns the hour value of this UTC offset.
    pub fn hour(&self) -> u8 {
        match self {
            Self::MinutePrecision(offset) => offset.hour,
            Self::FullPrecisionOffset(offset) => offset.minute_precision_offset.hour,
        }
    }

    /// Returns the minute value of this UTC offset.
    pub fn minute(&self) -> u8 {
        match self {
            Self::MinutePrecision(offset) => offset.minute,
            Self::FullPrecisionOffset(offset) => offset.minute_precision_offset.minute,
        }
    }

    /// Returns the second value of this UTC offset if it is a full precision offset.
    pub fn second(&self) -> Option<u8> {
        match self {
            Self::MinutePrecision(_) => None,
            Self::FullPrecisionOffset(offset) => Some(offset.second),
        }
    }

    /// Returns the fraction value of this UTC offset if it is a full precision offset.
    pub fn fraction(&self) -> Option<Fraction> {
        match self {
            Self::MinutePrecision(_) => None,
            Self::FullPrecisionOffset(offset) => offset.fraction,
        }
    }
}

/// A minute preicision UTC offset
#[derive(Debug, Clone, Copy, PartialEq)]
#[allow(clippy::exhaustive_structs)] // Minute precision offset can only be a sign, hour and minute field
pub struct MinutePrecisionOffset {
    /// The `Sign` value of the `UtcOffsetRecord`.
    pub sign: Sign,
    /// The hour value of the `UtcOffsetRecord`.
    pub hour: u8,
    /// The minute value of the `UtcOffsetRecord`.
    pub minute: u8,
}

/// A full precision UTC offset represented by a `MinutePrecisionOffset`
/// with seconds and an optional fractional seconds
#[derive(Debug, Clone, Copy, PartialEq)]
#[allow(clippy::exhaustive_structs)] // Full precision offset is only these five component fields
pub struct FullPrecisionOffset {
    /// The minute precision offset of a `FullPrecisionOffset`.
    pub minute_precision_offset: MinutePrecisionOffset,
    /// The second value of a `FullPrecisionOffset`.
    pub second: u8,
    /// Any nanosecond value of a `FullPrecisionOffset`.
    pub fraction: Option<Fraction>,
}

impl MinutePrecisionOffset {
    /// +0000
    pub const fn zero() -> Self {
        Self {
            sign: Sign::Positive,
            hour: 0,
            minute: 0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
#[allow(clippy::exhaustive_enums)] // explicitly A or B
pub enum UtcOffsetRecordOrZ {
    Offset(UtcOffsetRecord),
    Z,
}

impl UtcOffsetRecordOrZ {
    /// Resolves to a [`UtcOffsetRecord`] according to RFC9557: "Z" == "-00:00"
    pub fn resolve_rfc_9557(self) -> UtcOffsetRecord {
        match self {
            UtcOffsetRecordOrZ::Offset(o) => o,
            UtcOffsetRecordOrZ::Z => UtcOffsetRecord::MinutePrecision(MinutePrecisionOffset {
                sign: Sign::Negative,
                hour: 0,
                minute: 0,
            }),
        }
    }
}

/// The resulting record of parsing a `Duration` string.
#[allow(clippy::exhaustive_structs)]
// A duration can only be a Sign, a DateDuration part, and a TimeDuration part that users need to match on.
#[cfg(feature = "duration")]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct DurationParseRecord {
    /// Duration Sign
    pub sign: Sign,
    /// The parsed `DateDurationRecord` if present.
    pub date: Option<DateDurationRecord>,
    /// The parsed `TimeDurationRecord` if present.
    pub time: Option<TimeDurationRecord>,
}

/// A `DateDurationRecord` represents the result of parsing the date component of a Duration string.
#[allow(clippy::exhaustive_structs)]
// A `DateDurationRecord` by spec can only be made up of years, months, weeks, and days parts that users need to match on.
#[cfg(feature = "duration")]
#[derive(Default, Debug, Clone, Copy, PartialEq)]
pub struct DateDurationRecord {
    /// Years value.
    pub years: u32,
    /// Months value.
    pub months: u32,
    /// Weeks value.
    pub weeks: u32,
    /// Days value.
    pub days: u64,
}

/// A `TimeDurationRecord` represents the result of parsing the time component of a Duration string.
#[allow(clippy::exhaustive_enums)]
// A `TimeDurationRecord` by spec can only be made up of the valid parts up to a present fraction that users need to match on.
#[cfg(feature = "duration")]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum TimeDurationRecord {
    // An hours Time duration record.
    Hours {
        /// Hours value.
        hours: u64,
        /// The parsed fractional digits.
        fraction: Option<Fraction>,
    },
    // A Minutes Time duration record.
    Minutes {
        /// Hours value.
        hours: u64,
        /// Minutes value.
        minutes: u64,
        /// The parsed fractional digits.
        fraction: Option<Fraction>,
    },
    // A Seconds Time duration record.
    Seconds {
        /// Hours value.
        hours: u64,
        /// Minutes value.
        minutes: u64,
        /// Seconds value.
        seconds: u64,
        /// The parsed fractional digits.
        fraction: Option<Fraction>,
    },
}

/// A fraction value in nanoseconds or lower value.
///
/// # Precision note
///
/// `ixdtf` parses a fraction value to a precision of 18 digits of precision, but
/// preserves the fraction's digit length
#[derive(Debug, Clone, Copy, PartialEq)]
#[allow(clippy::exhaustive_structs)] // A fraction is only a value and its digit length.
pub struct Fraction {
    // The count of fraction digits, i.e. the fraction's digit length.
    pub(crate) digits: NonZeroU8,
    // The parsed fraction value.
    pub(crate) value: u64,
}

impl Fraction {
    /// Returns Some(`u32`) representing the `Fraction` as it's computed
    /// nanosecond value or `None` if the digits exceeds 9 digits.
    ///
    /// ```rust
    /// use ixdtf::parsers::IxdtfParser;
    ///
    /// // Fraction is below 9 digits.
    /// let fraction_str = "2025-02-17T05:41:32.12345678";
    /// let result = IxdtfParser::from_str(fraction_str).parse().unwrap();
    ///
    /// let time = result.time.unwrap();
    /// let fraction = time.fraction.unwrap();
    ///
    /// assert_eq!(fraction.to_nanoseconds(), Some(123456780));
    ///
    /// // Fraction is 10 digits.
    /// let fraction_str = "2025-02-17T05:41:32.1234567898";
    /// let result = IxdtfParser::from_str(fraction_str).parse().unwrap();
    /// let time = result.time.unwrap();
    /// let fraction = time.fraction.unwrap();
    ///
    /// assert_eq!(fraction.to_nanoseconds(), None);
    /// ```
    pub fn to_nanoseconds(&self) -> Option<u32> {
        if self.digits.get() <= 9 {
            Some(10u32.pow(9 - u32::from(self.digits.get())) * (self.value as u32))
        } else {
            None
        }
    }

    /// Returns a `u32` representing the `Fraction` as it's computed
    /// nanosecond value, truncating any value beyond 9 digits to
    /// nanoseconds.
    ///
    /// This method will return `None` if the value exceeds a represented
    /// range or the underlying `Fraction` is malformed.
    ///
    /// ```rust
    /// use ixdtf::parsers::IxdtfParser;
    ///
    /// // Fraction is 13 digits.
    /// let fraction_str = "2025-02-17T05:41:32.1234567898765";
    /// let result = IxdtfParser::from_str(fraction_str).parse().unwrap();
    ///
    /// let time = result.time.unwrap();
    /// let fraction = time.fraction.unwrap();
    ///
    /// assert_eq!(fraction.to_truncated_nanoseconds(), 123456789);
    /// assert_eq!(fraction.to_nanoseconds(), None);
    /// ```
    pub fn to_truncated_nanoseconds(&self) -> u32 {
        self.to_nanoseconds()
            .unwrap_or((self.value / 10u64.pow(u32::from(self.digits.get() - 9))) as u32)
    }
}
