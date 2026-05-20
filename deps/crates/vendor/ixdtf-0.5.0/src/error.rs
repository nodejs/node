// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! An error enum for representing `ixdtf` parsing errors.

use displaydoc::Display;

#[non_exhaustive]
#[derive(PartialEq, Display, Clone, Copy, Debug)]
/// The error returned by `ixdtf`'s parsers.
pub enum ParseError {
    #[displaydoc("Implementation error: this error must not throw.")]
    ImplAssert,
    #[displaydoc("Invalid float while parsing fraction part.")]
    ParseFloat,
    #[displaydoc("Parsing ended abruptly while parsing {location}")]
    AbruptEnd { location: &'static str },
    #[displaydoc("Unexpected character found after parsing was completed.")]
    InvalidEnd,

    // Date related errors
    #[displaydoc("Parsed month value not in a valid range.")]
    InvalidMonthRange,
    #[displaydoc("Parsed day value not in a valid range.")]
    InvalidDayRange,
    #[displaydoc("Invalid chracter while parsing year value.")]
    DateYear,
    #[displaydoc("Invalid character while parsing extended year value.")]
    DateExtendedYear,
    #[displaydoc("Invalid caracter while parsing month value.")]
    DateMonth,
    #[displaydoc("Invalid character while parsing day value.")]
    DateDay,
    #[displaydoc("Unexpected end while parsing a date value.")]
    DateUnexpectedEnd,

    // Time Related errors
    #[displaydoc("Time is required.")]
    TimeRequired,
    #[displaydoc("Invalid character while parsing hour value.")]
    TimeHour,
    #[displaydoc("Invalid character while parsing minute/second value in (0, 59] range.")]
    TimeMinuteSecond,
    #[displaydoc("Invalid character while parsing second value in (0, 60] range.")]
    TimeSecond,
    #[displaydoc("Invalid character while parsing fraction part value.")]
    FractionPart,
    #[displaydoc("Invalid character while parsing date separator.")]
    DateSeparator,
    #[displaydoc("Invalid character while parsing time separator.")]
    TimeSeparator,
    #[displaydoc("Invalid character while parsing decimal separator.")]
    DecimalSeparator,

    // Annotation Related Errors
    #[displaydoc("Invalid annotation.")]
    InvalidAnnotation,
    #[displaydoc("Invalid annotation open character.")]
    AnnotationOpen,
    #[displaydoc("Invalid annotation close character.")]
    AnnotationClose,
    #[displaydoc("Invalid annotation character.")]
    AnnotationChar,
    #[displaydoc("Invalid annotation key-value separator character.")]
    AnnotationKeyValueSeparator,
    #[displaydoc("Invalid annotation key leading character.")]
    AnnotationKeyLeadingChar,
    #[displaydoc("Invalid annotation key character.")]
    AnnotationKeyChar,
    #[displaydoc("Expected annotation value character must exist after hyphen.")]
    AnnotationValueCharPostHyphen,
    #[displaydoc("Invalid annotation value character.")]
    AnnotationValueChar,

    // Duplicate calendar with critical.
    #[displaydoc("Duplicate calendars cannot be provided when one is critical.")]
    CriticalDuplicateCalendar,
    #[displaydoc("Unrecognized annoation is marked as critical.")]
    UnrecognizedCritical,

    // Time Zone Errors
    #[displaydoc("Invalid time zone leading character.")]
    TzLeadingChar,
    #[displaydoc("Expected time zone character after '/'.")]
    IanaCharPostSeparator,
    #[displaydoc("Invalid IANA time zone character after '/'.")]
    IanaChar,
    #[displaydoc("Invalid time zone character after '/'.")]
    UtcTimeSeparator,

    // MonthDay Errors
    #[displaydoc("MonthDay must begin with a month or '--'")]
    MonthDayHyphen,

    // Duration Errors
    #[displaydoc("Invalid duration designator.")]
    DurationDisgnator,
    #[displaydoc("Provided Duration field value exceeds supported range.")]
    DurationValueExceededRange,
    #[displaydoc("Invalid date duration part order.")]
    DateDurationPartOrder,
    #[displaydoc("Invalid time duration part order.")]
    TimeDurationPartOrder,
    #[displaydoc("Invalid time duration designator.")]
    TimeDurationDesignator,
}

impl core::error::Error for ParseError {}

impl ParseError {
    pub(crate) fn abrupt_end(location: &'static str) -> Self {
        ParseError::AbruptEnd { location }
    }
}
