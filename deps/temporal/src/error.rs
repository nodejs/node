//! This module implements `TemporalError`.

use core::fmt;
use ixdtf::ParseError;
use timezone_provider::TimeZoneProviderError;

use icu_calendar::DateError;

/// `TemporalError`'s error type.
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub enum ErrorKind {
    /// Error.
    #[default]
    Generic,
    /// TypeError
    Type,
    /// RangeError
    Range,
    /// SyntaxError
    Syntax,
    /// Assert
    Assert,
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Generic => "Error",
            Self::Type => "TypeError",
            Self::Range => "RangeError",
            Self::Syntax => "SyntaxError",
            Self::Assert => "ImplementationError",
        }
        .fmt(f)
    }
}

/// The error type for `boa_temporal`.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct TemporalError {
    kind: ErrorKind,
    msg: ErrorMessage,
}

impl TemporalError {
    #[inline]
    #[must_use]
    const fn new(kind: ErrorKind) -> Self {
        Self {
            kind,
            msg: ErrorMessage::None,
        }
    }

    /// Create a generic error
    #[inline]
    #[must_use]
    pub fn general(msg: &'static str) -> Self {
        Self::new(ErrorKind::Generic).with_message(msg)
    }

    /// Create a range error.
    #[inline]
    #[must_use]
    pub const fn range() -> Self {
        Self::new(ErrorKind::Range)
    }

    /// Create a type error.
    #[inline]
    #[must_use]
    pub const fn r#type() -> Self {
        Self::new(ErrorKind::Type)
    }

    /// Create a syntax error.
    #[inline]
    #[must_use]
    pub const fn syntax() -> Self {
        Self::new(ErrorKind::Syntax)
    }

    /// Creates an assertion error
    #[inline]
    #[must_use]
    #[cfg_attr(debug_assertions, track_caller)]
    pub(crate) const fn assert() -> Self {
        #[cfg(not(debug_assertions))]
        {
            Self::new(ErrorKind::Assert)
        }
        #[cfg(debug_assertions)]
        Self {
            kind: ErrorKind::Assert,
            msg: ErrorMessage::String(core::panic::Location::caller().file()),
        }
    }

    /// Create an abrupt end error.
    #[inline]
    #[must_use]
    pub fn abrupt_end() -> Self {
        Self::syntax().with_message("Abrupt end to parsing target.")
    }

    /// Add a message to the error.
    #[inline]
    #[must_use]
    pub fn with_message(mut self, msg: &'static str) -> Self {
        self.msg = ErrorMessage::String(msg);
        self
    }

    /// Add a message enum to the error.
    #[inline]
    #[must_use]
    pub(crate) fn with_enum(mut self, msg: ErrorMessage) -> Self {
        self.msg = msg;
        self
    }

    /// Returns this error's kind.
    #[inline]
    #[must_use]
    pub const fn kind(&self) -> ErrorKind {
        self.kind
    }

    /// Extracts the error message.
    #[inline]
    #[must_use]
    pub fn into_message(self) -> &'static str {
        self.msg.to_string()
    }
}

impl fmt::Display for TemporalError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.kind)?;

        let msg = self.msg.to_string();
        if !msg.is_empty() {
            write!(f, ": {msg}")?;
        }

        Ok(())
    }
}

impl From<DateError> for TemporalError {
    fn from(error: DateError) -> Self {
        TemporalError::range().with_enum(ErrorMessage::Icu4xDate(error))
    }
}

impl From<ParseError> for TemporalError {
    fn from(error: ParseError) -> Self {
        TemporalError::range().with_enum(ErrorMessage::Ixdtf(error))
    }
}

/// The error message
#[derive(Clone, Copy, PartialEq, Debug)]
pub(crate) enum ErrorMessage {
    // Range
    InstantOutOfRange,
    IntermediateDateTimeOutOfRange,
    ZDTOutOfDayBounds,
    LargestUnitCannotBeDateUnit,
    DateOutOfRange,

    // Numerical errors
    NumberNotFinite,
    NumberNotIntegral,
    NumberNotPositive,
    NumberOutOfRange,
    FractionalDigitsPrecisionInvalid,

    // Options validity
    SmallestUnitIsRequired,
    SmallestUnitNotTimeUnit,
    SmallestUnitLargerThanLargestUnit,
    UnitNotDate,
    UnitNotTime,
    UnitRequired,
    UnitNoAutoDuringComparison,
    RoundToUnitInvalid,
    RoundingModeInvalid,
    CalendarNameInvalid,
    OffsetOptionInvalid,
    TimeZoneNameInvalid,

    // Field mismatches
    CalendarMismatch,
    TzMismatch,

    // Parsing
    ParserNeedsDate,
    FractionalTimeMoreThanNineDigits,

    // Other
    OffsetNeedsDisambiguation,

    // Typed
    None,
    String(&'static str),
    Ixdtf(ParseError),
    Icu4xDate(DateError),
}

impl ErrorMessage {
    pub fn to_string(self) -> &'static str {
        match self {
            Self::InstantOutOfRange => "Instant nanoseconds are not within a valid epoch range.",
            Self::IntermediateDateTimeOutOfRange => {
                "Intermediate ISO datetime was not within a valid range."
            }
            Self::ZDTOutOfDayBounds => "ZonedDateTime is outside the expected day bounds",
            Self::LargestUnitCannotBeDateUnit => "Largest unit cannot be a date unit",
            Self::DateOutOfRange => "Date is not within ISO date time limits.",
            Self::NumberNotFinite => "number value is not a finite value.",
            Self::NumberNotIntegral => "value must be integral.",
            Self::NumberNotPositive => "integer must be positive.",
            Self::NumberOutOfRange => "number exceeded a valid range.",
            Self::FractionalDigitsPrecisionInvalid => "Invalid fractionalDigits precision value",
            Self::SmallestUnitIsRequired => "smallestUnit is required",
            Self::SmallestUnitNotTimeUnit => "smallestUnit must be a valid time unit.",
            Self::SmallestUnitLargerThanLargestUnit => {
                "smallestUnit was larger than largestunit in DifferenceeSettings"
            }
            Self::UnitNotDate => "Unit was not part of the date unit group.",
            Self::UnitNotTime => "Unit was not part of the time unit group.",
            Self::UnitRequired => "Unit is required",
            Self::UnitNoAutoDuringComparison => "'auto' units are not allowed during comparison",
            Self::RoundToUnitInvalid => "Invalid roundTo unit provided.",
            Self::RoundingModeInvalid => "Invalid roundingMode option provided",
            Self::CalendarNameInvalid => "Invalid calendarName option provided",
            Self::OffsetOptionInvalid => "Invalid offsetOption option provided",
            Self::TimeZoneNameInvalid => "Invalid timeZoneName option provided",
            Self::CalendarMismatch => {
                "Calendar must be the same for operations involving two calendared types."
            }
            Self::TzMismatch => "Timezones must be the same if unit is a day unit.",

            Self::ParserNeedsDate => "Could not find a valid DateRecord node during parsing.",
            Self::FractionalTimeMoreThanNineDigits => "Fractional time exceeds nine digits.",
            Self::OffsetNeedsDisambiguation => {
                "Offsets could not be determined without disambiguation"
            }
            Self::None => "",
            Self::String(s) => s,
            Self::Ixdtf(s) => ixdtf_error_to_static_string(s),
            Self::Icu4xDate(DateError::Range { field, .. }) => match field {
                "year" => "Year out of range.",
                "month" => "Month out of range.",
                "day" => "Day out of range.",
                _ => "Field out of range.",
            },
            Self::Icu4xDate(DateError::UnknownEra) => "Unknown era.",
            Self::Icu4xDate(DateError::UnknownMonthCode(..)) => "Unknown month code.",
            Self::Icu4xDate(_) => "Date error.",
        }
    }
}

impl From<TimeZoneProviderError> for TemporalError {
    fn from(other: TimeZoneProviderError) -> Self {
        match other {
            TimeZoneProviderError::InstantOutOfRange => {
                Self::range().with_enum(ErrorMessage::InstantOutOfRange)
            }
            TimeZoneProviderError::Assert(s) => Self::assert().with_message(s),
            TimeZoneProviderError::Range(s) => Self::range().with_message(s),
            _ => Self::assert().with_message("Unknown TimeZoneProviderError"),
        }
    }
}

// ICU4X will get this API natively eventually
// https://github.com/unicode-org/icu4x/issues/6904
pub fn ixdtf_error_to_static_string(error: ParseError) -> &'static str {
    match error {
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

        ParseError::TimeSecond => "Invalid character while parsing second value in (0, 60] range.",

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
        _ => "General IXDTF parsing error",
    }
}
