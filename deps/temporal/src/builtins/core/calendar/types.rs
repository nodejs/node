//! Implementation of `ResolvedCalendarFields`

use tinystr::tinystr;
use tinystr::TinyAsciiStr;

use crate::fields::CalendarFields;
use crate::iso::{constrain_iso_day, is_valid_iso_day};
use crate::options::Overflow;
use crate::{TemporalError, TemporalResult};
use icu_calendar::AnyCalendarKind;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ResolutionType {
    Date,
    YearMonth,
    MonthDay,
}

/// `ResolvedCalendarFields` represents the resolved field values necessary for
/// creating a Date from potentially partial values.
#[derive(Debug)]
pub struct ResolvedIsoFields {
    pub(crate) arithmetic_year: i32,
    pub(crate) month: u8,
    pub(crate) day: u8,
}

impl ResolvedIsoFields {
    // TODO: Potentially make a method on `Calendar`.
    /// <https://tc39.es/proposal-temporal/#sec-temporal-calendarresolvefields>
    #[inline]
    pub fn try_from_fields(
        fields: &CalendarFields,
        overflow: Overflow,
        resolve_type: ResolutionType,
    ) -> TemporalResult<Self> {
        fields.check_year_in_safe_arithmetical_range()?;
        // a. If type is date or year-month and fields.[[Year]] is unset, throw a TypeError exception.
        let arithmetic_year = if resolve_type == ResolutionType::MonthDay {
            1972
        } else {
            // ISO does not look at era/eraYear at all
            fields
                .year
                .ok_or(TemporalError::r#type().with_message("Required year field is empty."))?
        };
        // b. If type is date or month-day and fields.[[Day]] is unset, throw a TypeError exception.
        let day = if resolve_type == ResolutionType::YearMonth {
            1
        } else {
            fields
                .day
                .ok_or(TemporalError::r#type().with_message("Required day field is empty."))?
        };

        let mut month = match (fields.month, fields.month_code) {
            (Some(month), None) => month,
            (ordinal, Some(code)) => {
                code.validate(AnyCalendarKind::Iso)?;
                if code.is_leap_month() {
                    return Err(TemporalError::range()
                        .with_message("No leap months allowed for ISO calendar"));
                }
                let found_ordinal = code.to_month_integer();

                if let Some(ordinal) = ordinal {
                    if ordinal != found_ordinal {
                        return Err(
                            TemporalError::range().with_message("Month does not match monthCode.")
                        );
                    }
                }
                found_ordinal
            }
            (None, None) => {
                return Err(TemporalError::r#type()
                    .with_message("Required month/monthCode field is empty."))
            }
        };

        if !(1..=12).contains(&month) {
            if overflow == Overflow::Constrain {
                month = if month > 12 { 12 } else { 1 };
            } else {
                return Err(TemporalError::range().with_message("Month out of range."));
            }
        }

        let day = if overflow == Overflow::Constrain {
            constrain_iso_day(arithmetic_year, month, day)
        } else {
            if !is_valid_iso_day(arithmetic_year, month, day) {
                return Err(
                    TemporalError::range().with_message("day value is not in a valid range.")
                );
            }
            day
        };
        Ok(Self {
            arithmetic_year,
            month,
            day,
        })
    }
}

// MonthCode constants.
const MONTH_ONE: TinyAsciiStr<4> = tinystr!(4, "M01");
const MONTH_ONE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M01L");
const MONTH_TWO: TinyAsciiStr<4> = tinystr!(4, "M02");
const MONTH_TWO_LEAP: TinyAsciiStr<4> = tinystr!(4, "M02L");
const MONTH_THREE: TinyAsciiStr<4> = tinystr!(4, "M03");
const MONTH_THREE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M03L");
const MONTH_FOUR: TinyAsciiStr<4> = tinystr!(4, "M04");
const MONTH_FOUR_LEAP: TinyAsciiStr<4> = tinystr!(4, "M04L");
const MONTH_FIVE: TinyAsciiStr<4> = tinystr!(4, "M05");
const MONTH_FIVE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M05L");
const MONTH_SIX: TinyAsciiStr<4> = tinystr!(4, "M06");
const MONTH_SIX_LEAP: TinyAsciiStr<4> = tinystr!(4, "M06L");
const MONTH_SEVEN: TinyAsciiStr<4> = tinystr!(4, "M07");
const MONTH_SEVEN_LEAP: TinyAsciiStr<4> = tinystr!(4, "M07L");
const MONTH_EIGHT: TinyAsciiStr<4> = tinystr!(4, "M08");
const MONTH_EIGHT_LEAP: TinyAsciiStr<4> = tinystr!(4, "M08L");
const MONTH_NINE: TinyAsciiStr<4> = tinystr!(4, "M09");
const MONTH_NINE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M09L");
const MONTH_TEN: TinyAsciiStr<4> = tinystr!(4, "M10");
const MONTH_TEN_LEAP: TinyAsciiStr<4> = tinystr!(4, "M10L");
const MONTH_ELEVEN: TinyAsciiStr<4> = tinystr!(4, "M11");
const MONTH_ELEVEN_LEAP: TinyAsciiStr<4> = tinystr!(4, "M11L");
const MONTH_TWELVE: TinyAsciiStr<4> = tinystr!(4, "M12");
const MONTH_TWELVE_LEAP: TinyAsciiStr<4> = tinystr!(4, "M12L");
const MONTH_THIRTEEN: TinyAsciiStr<4> = tinystr!(4, "M13");

// TODO: Handle instances where month values may be outside of valid
// bounds. In other words, it is totally possible for a value to be
// passed in that is { month: 300 } with overflow::constrain.
/// A MonthCode identifier
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct MonthCode(pub(crate) TinyAsciiStr<4>);

impl MonthCode {
    pub(crate) fn validate(&self, calendar: AnyCalendarKind) -> TemporalResult<()> {
        const COMMON_MONTH_CODES: [TinyAsciiStr<4>; 12] = [
            MONTH_ONE,
            MONTH_TWO,
            MONTH_THREE,
            MONTH_FOUR,
            MONTH_FIVE,
            MONTH_SIX,
            MONTH_SEVEN,
            MONTH_EIGHT,
            MONTH_NINE,
            MONTH_TEN,
            MONTH_ELEVEN,
            MONTH_TWELVE,
        ];

        const LUNAR_LEAP_MONTHS: [TinyAsciiStr<4>; 12] = [
            MONTH_ONE_LEAP,
            MONTH_TWO_LEAP,
            MONTH_THREE_LEAP,
            MONTH_FOUR_LEAP,
            MONTH_FIVE_LEAP,
            MONTH_SIX_LEAP,
            MONTH_SEVEN_LEAP,
            MONTH_EIGHT_LEAP,
            MONTH_NINE_LEAP,
            MONTH_TEN_LEAP,
            MONTH_ELEVEN_LEAP,
            MONTH_TWELVE_LEAP,
        ];

        if COMMON_MONTH_CODES.contains(&self.0) {
            return Ok(());
        }

        match calendar {
            AnyCalendarKind::Chinese | AnyCalendarKind::Dangi
                if LUNAR_LEAP_MONTHS.contains(&self.0) =>
            {
                Ok(())
            }
            AnyCalendarKind::Coptic
            | AnyCalendarKind::Ethiopian
            | AnyCalendarKind::EthiopianAmeteAlem
                if MONTH_THIRTEEN == self.0 =>
            {
                Ok(())
            }
            AnyCalendarKind::Hebrew if MONTH_FIVE_LEAP == self.0 => Ok(()),
            _ => Err(TemporalError::range()
                .with_message("MonthCode was not valid for the current calendar.")),
        }
    }

    /// Returns the `MonthCode` as an integer
    pub fn to_month_integer(&self) -> u8 {
        // Sometimes icu_calendar returns "und"
        // when the month is calculated to be out of range (usually for
        // out-of-astronomic range Islamic and Chinese calendars)
        //
        // Normalize to something sensible, since ascii_four_to_integer
        // will assert for non-digits.
        if self.0 == tinystr!(4, "und") {
            return 13;
        }
        ascii_four_to_integer(self.0)
    }

    /// Returns whether the `MonthCode` is a leap month.
    pub fn is_leap_month(&self) -> bool {
        let bytes = self.0.all_bytes();
        bytes[3] == b'L'
    }

    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }

    pub fn as_tinystr(&self) -> TinyAsciiStr<4> {
        self.0
    }

    pub fn try_from_utf8(src: &[u8]) -> TemporalResult<Self> {
        if !(3..=4).contains(&src.len()) {
            return Err(
                TemporalError::range().with_message("Month codes must have 3 or 4 characters.")
            );
        }

        let inner = TinyAsciiStr::<4>::try_from_utf8(src).map_err(|_e| TemporalError::range())?;

        let bytes = inner.all_bytes();
        if bytes[0] != b'M' {
            return Err(
                TemporalError::range().with_message("First month code character must be 'M'.")
            );
        }
        if !bytes[1].is_ascii_digit() || !bytes[2].is_ascii_digit() {
            return Err(TemporalError::range().with_message("Invalid month code digit."));
        }
        if src.len() == 4 && bytes[3] != b'L' {
            return Err(TemporalError::range().with_message("Leap month code must end with 'L'."));
        }

        Ok(Self(inner))
    }
}

impl core::str::FromStr for MonthCode {
    type Err = TemporalError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::try_from_utf8(s.as_bytes())
    }
}

// NOTE: This is a greedy function, should handle differently for all calendars.
#[inline]
#[cfg(test)]
pub(crate) fn month_to_month_code(month: u8) -> TemporalResult<MonthCode> {
    if !(1..=13).contains(&month) {
        return Err(TemporalError::range().with_message("Month not in a valid range."));
    }
    let first = month / 10;
    let second = month % 10;
    let tinystr = TinyAsciiStr::<4>::try_from_raw([b'M', first + 48, second + 48, b'\0'])
        .map_err(|_| TemporalError::range().with_message("Invalid month code"))?;
    Ok(MonthCode(tinystr))
}

// Potentially greedy. Need to verify for all calendars that
// the month code integer aligns with the month integer, which
// may require calendar info
#[inline]
fn ascii_four_to_integer(mc: TinyAsciiStr<4>) -> u8 {
    let bytes = mc.all_bytes();
    // Invariant: second and third character (index 1 and 2) are ascii digits.
    debug_assert!(bytes[1].is_ascii_digit());
    debug_assert!(bytes[2].is_ascii_digit());
    let first = ascii_digit_to_int(bytes[1]) * 10;
    first + ascii_digit_to_int(bytes[2])
}

#[inline]
const fn ascii_digit_to_int(ascii_digit: u8) -> u8 {
    ascii_digit - 48
}

#[cfg(test)]
mod tests {
    use core::str::FromStr;

    use tinystr::tinystr;

    use crate::{
        builtins::{
            calendar::{types::ResolutionType, CalendarFields},
            core::calendar::Calendar,
        },
        options::Overflow,
    };

    use super::{MonthCode, ResolvedIsoFields};

    #[test]
    fn valid_month_code() {
        let month_code = MonthCode::from_str("M01").unwrap();
        assert!(!month_code.is_leap_month());
        assert_eq!(month_code.to_month_integer(), 1);

        let month_code = MonthCode::from_str("M12").unwrap();
        assert!(!month_code.is_leap_month());
        assert_eq!(month_code.to_month_integer(), 12);

        let month_code = MonthCode::from_str("M13L").unwrap();
        assert!(month_code.is_leap_month());
        assert_eq!(month_code.to_month_integer(), 13);
    }

    #[test]
    fn invalid_month_code() {
        let _ = MonthCode::from_str("01").unwrap_err();
        let _ = MonthCode::from_str("N01").unwrap_err();
        let _ = MonthCode::from_str("M01R").unwrap_err();
        let _ = MonthCode::from_str("M1").unwrap_err();
        let _ = MonthCode::from_str("M1L").unwrap_err();
    }

    #[test]
    fn day_overflow_test() {
        let bad_fields = CalendarFields {
            year: Some(2019),
            month: Some(1),
            day: Some(32),
            ..Default::default()
        };

        let cal = Calendar::default();

        let err = cal.date_from_fields(bad_fields.clone(), Overflow::Reject);
        assert!(err.is_err());
        let result = cal.date_from_fields(bad_fields, Overflow::Constrain);
        assert!(result.is_ok());
    }

    #[test]
    fn unresolved_month_and_month_code() {
        let bad_fields = CalendarFields {
            year: Some(1976),
            month: Some(11),
            month_code: Some(MonthCode(tinystr!(4, "M12"))),
            day: Some(18),
            ..Default::default()
        };

        let err =
            ResolvedIsoFields::try_from_fields(&bad_fields, Overflow::Reject, ResolutionType::Date);
        assert!(err.is_err());
    }

    #[test]
    fn missing_partial_fields() {
        let bad_fields = CalendarFields {
            year: Some(2019),
            day: Some(19),
            ..Default::default()
        };

        let err =
            ResolvedIsoFields::try_from_fields(&bad_fields, Overflow::Reject, ResolutionType::Date);
        assert!(err.is_err());

        let bad_fields = CalendarFields::default();
        let err =
            ResolvedIsoFields::try_from_fields(&bad_fields, Overflow::Reject, ResolutionType::Date);
        assert!(err.is_err());
    }
}
