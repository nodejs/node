// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! An error enum for representing `ixdtf` parsing errors.

use core::fmt;

#[non_exhaustive]
#[derive(PartialEq, Clone, Copy, Debug)]
/// The error returned by `ixdtf`'s parsers.
pub enum ParseError {
    ImplAssert,
    NonAsciiCodePoint,
    ParseFloat,
    AbruptEnd { location: &'static str },
    InvalidEnd,

    // Date related errors
    InvalidMonthRange,
    InvalidDayRange,
    DateYear,
    DateExtendedYear,
    DateMonth,
    DateDay,
    DateUnexpectedEnd,

    // Time Related errors
    TimeRequired,
    TimeHour,
    TimeMinuteSecond,
    TimeSecond,
    FractionPart,
    DateSeparator,
    TimeSeparator,
    DecimalSeparator,

    // Annotation Related Errors
    InvalidAnnotation,
    AnnotationOpen,
    AnnotationClose,
    AnnotationChar,
    AnnotationKeyValueSeparator,
    AnnotationKeyLeadingChar,
    AnnotationKeyChar,
    AnnotationValueCharPostHyphen,
    AnnotationValueChar,
    InvalidMinutePrecisionOffset,

    // Duplicate calendar with critical.
    CriticalDuplicateCalendar,
    UnrecognizedCritical,

    // Time Zone Errors
    TzLeadingChar,
    IanaCharPostSeparator,
    IanaChar,
    UtcTimeSeparator,
    OffsetNeedsSign,

    // MonthDay Errors
    MonthDayHyphen,

    // Duration Errors
    DurationDisgnator,
    DurationValueExceededRange,
    DateDurationPartOrder,
    TimeDurationPartOrder,
    TimeDurationDesignator,

    AmbiguousTimeMonthDay,
    AmbiguousTimeYearMonth,
    InvalidMonthDay,
}

impl core::error::Error for ParseError {}

impl ParseError {
    pub(crate) fn abrupt_end(location: &'static str) -> Self {
        ParseError::AbruptEnd { location }
    }

    /// Convert this error to a static string representation
    pub fn to_static_string(&self) -> &'static str {
        match *self {
            ParseError::ImplAssert => "Implementation error: this error must not throw.",

            ParseError::NonAsciiCodePoint => "Code point was not ASCII",

            ParseError::ParseFloat => "Invalid float while parsing fraction part.",

            ParseError::AbruptEnd { .. } => "Parsing ended abruptly.",

            ParseError::InvalidEnd => "Unexpected character found after parsing was completed.",
            // Date related errors
            ParseError::InvalidMonthRange => "Parsed month value not in a valid range.",

            ParseError::InvalidDayRange => "Parsed day value not in a valid range.",

            ParseError::DateYear => "Invalid chracter while parsing year value.",

            ParseError::DateExtendedYear => "Invalid character while parsing extended year value.",

            ParseError::DateMonth => "Invalid character while parsing month value.",

            ParseError::DateDay => "Invalid character while parsing day value.",

            ParseError::DateUnexpectedEnd => "Unexpected end while parsing a date value.",

            ParseError::TimeRequired => "Time is required.",

            ParseError::TimeHour => "Invalid character while parsing hour value.",

            ParseError::TimeMinuteSecond => {
                "Invalid character while parsing minute/second value in (0, 59] range."
            }

            ParseError::TimeSecond => {
                "Invalid character while parsing second value in (0, 60] range."
            }

            ParseError::FractionPart => "Invalid character while parsing fraction part value.",

            ParseError::DateSeparator => "Invalid character while parsing date separator.",

            ParseError::TimeSeparator => "Invalid character while parsing time separator.",

            ParseError::DecimalSeparator => "Invalid character while parsing decimal separator.",
            // Annotation Related Errors
            ParseError::InvalidAnnotation => "Invalid annotation.",

            ParseError::AnnotationOpen => "Invalid annotation open character.",

            ParseError::AnnotationClose => "Invalid annotation close character.",

            ParseError::AnnotationChar => "Invalid annotation character.",

            ParseError::AnnotationKeyValueSeparator => {
                "Invalid annotation key-value separator character."
            }

            ParseError::AnnotationKeyLeadingChar => "Invalid annotation key leading character.",

            ParseError::AnnotationKeyChar => "Invalid annotation key character.",

            ParseError::AnnotationValueCharPostHyphen => {
                "Expected annotation value character must exist after hyphen."
            }

            ParseError::AnnotationValueChar => "Invalid annotation value character.",

            ParseError::InvalidMinutePrecisionOffset => "Offset must be minute precision",

            ParseError::CriticalDuplicateCalendar => {
                "Duplicate calendars cannot be provided when one is critical."
            }

            ParseError::UnrecognizedCritical => "Unrecognized annoation is marked as critical.",

            ParseError::TzLeadingChar => "Invalid time zone leading character.",

            ParseError::IanaCharPostSeparator => "Expected time zone character after '/'.",

            ParseError::IanaChar => "Invalid IANA time zone character after '/'.",

            ParseError::UtcTimeSeparator => "Invalid time zone character after '/'.",

            ParseError::OffsetNeedsSign => "UTC offset needs a sign",

            ParseError::MonthDayHyphen => "MonthDay must begin with a month or '--'",

            ParseError::DurationDisgnator => "Invalid duration designator.",

            ParseError::DurationValueExceededRange => {
                "Provided Duration field value exceeds supported range."
            }

            ParseError::DateDurationPartOrder => "Invalid date duration part order.",

            ParseError::TimeDurationPartOrder => "Invalid time duration part order.",

            ParseError::TimeDurationDesignator => "Invalid time duration designator.",

            ParseError::AmbiguousTimeMonthDay => "Time is ambiguous with MonthDay",

            ParseError::AmbiguousTimeYearMonth => "Time is ambiguous with YearMonth",

            ParseError::InvalidMonthDay => "MonthDay was not valid.",
        }
    }
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if let ParseError::AbruptEnd { location } = *self {
            write!(f, "Parsing ended abruptly while parsing {location}")
        } else {
            f.write_str(self.to_static_string())
        }
    }
}
