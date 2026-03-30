use tinystr::TinyAsciiStr;

use super::types::month_to_month_code;
use crate::{
    error::ErrorMessage, options::Overflow, Calendar, MonthCode, PlainDate, PlainDateTime,
    PlainMonthDay, PlainYearMonth, TemporalError, TemporalResult,
};
use core::ops::Range;

/// The return value of CalendarFieldKeysToIgnore
#[derive(Copy, Clone, Default)]
pub(crate) struct FieldKeysToIgnore {
    /// Ignore all era fields (era-year, era)
    pub era: bool,
    /// Ignore arithmetical year
    pub arithmetical_year: bool,
    /// Ignore all month fields (month, month-code)
    pub month: bool,
}

#[derive(Debug, Default, Clone, PartialEq)]
pub struct CalendarFields {
    // A potentially set `year` field.
    pub year: Option<i32>,
    // A potentially set `month` field.
    pub month: Option<u8>,
    // A potentially set `month_code` field.
    pub month_code: Option<MonthCode>,
    // A potentially set `day` field.
    pub day: Option<u8>,
    // A potentially set `era` field.
    pub era: Option<TinyAsciiStr<19>>,
    // A potentially set `era_year` field.
    pub era_year: Option<i32>,
}

impl CalendarFields {
    pub const fn new() -> Self {
        Self {
            year: None,
            month: None,
            month_code: None,
            day: None,
            era: None,
            era_year: None,
        }
    }

    /// Temporal dates are valid between ISO -271821-04-20 and +275760-09-13,
    /// but this may not correspond to the same thing for other calendars.
    ///
    /// These year values are well in range for not needing to worry about overflow/underflow,
    /// but we cannot check for them without converting to the calendar first (via ISO).
    ///
    /// This method provides a quick and easy way to check that the years are in a safe arithmetic
    /// range without necessarily needing to resolve calendar specific stuff.
    ///
    /// This is primarily a defense in depth mechanism; we should still use saturating/checked ops
    /// where possible. This works nicely since we have an unambiguous answer for "GIGO or error"
    /// since this check can always produce an error when out of range.
    pub(crate) fn check_year_in_safe_arithmetical_range(&self) -> TemporalResult<()> {
        // No calendars have eras offset more than 6000 years from the ISO epoch,
        // and we generously round it up to ±300k
        const ROUGH_YEAR_RANGE: Range<i32> = -300000..300000;
        if let Some(year) = self.year {
            if !ROUGH_YEAR_RANGE.contains(&year) {
                return Err(TemporalError::range().with_enum(ErrorMessage::DateOutOfRange));
            }
        }
        if let Some(era_year) = self.era_year {
            if !ROUGH_YEAR_RANGE.contains(&era_year) {
                return Err(TemporalError::range().with_enum(ErrorMessage::DateOutOfRange));
            }
        }
        Ok(())
    }

    pub const fn with_era(mut self, era: Option<TinyAsciiStr<19>>) -> Self {
        self.era = era;
        self
    }

    pub const fn with_era_year(mut self, era_year: Option<i32>) -> Self {
        self.era_year = era_year;
        self
    }

    pub const fn with_year(mut self, year: i32) -> Self {
        self.year = Some(year);
        self
    }

    pub const fn with_optional_year(mut self, year: Option<i32>) -> Self {
        self.year = year;
        self
    }

    pub const fn with_month(mut self, month: u8) -> Self {
        self.month = Some(month);
        self
    }

    pub const fn with_optional_month(mut self, month: Option<u8>) -> Self {
        self.month = month;
        self
    }

    pub const fn with_month_code(mut self, month_code: MonthCode) -> Self {
        self.month_code = Some(month_code);
        self
    }

    pub const fn with_optional_month_code(mut self, month_code: Option<MonthCode>) -> Self {
        self.month_code = month_code;
        self
    }

    pub const fn with_day(mut self, day: u8) -> Self {
        self.day = Some(day);
        self
    }

    pub const fn with_optional_day(mut self, day: Option<u8>) -> Self {
        self.day = day;
        self
    }
}

impl CalendarFields {
    pub fn is_empty(&self) -> bool {
        *self == Self::new()
    }

    pub(crate) fn from_month_day(month_day: &PlainMonthDay) -> Self {
        Self {
            year: None,
            month: None,
            month_code: Some(month_day.month_code()),
            era: None,
            era_year: None,
            day: Some(month_day.day()),
        }
    }

    pub(crate) fn from_date(date: &PlainDate) -> Self {
        Self {
            year: Some(date.year()),
            month: Some(date.month()),
            month_code: Some(date.month_code()),
            era: date.era().map(TinyAsciiStr::resize),
            era_year: date.era_year(),
            day: Some(date.day()),
        }
    }

    crate::impl_with_fallback_method!(with_fallback_date, CalendarFields, (with_day: day) PlainDate);
    crate::impl_with_fallback_method!(with_fallback_datetime, CalendarFields, (with_day:day) PlainDateTime);
    crate::impl_field_keys_to_ignore!((with_day:day));
}

impl From<YearMonthCalendarFields> for CalendarFields {
    fn from(value: YearMonthCalendarFields) -> Self {
        Self {
            year: value.year,
            month: value.month,
            month_code: value.month_code,
            day: None,
            era: value.era,
            era_year: value.era_year,
        }
    }
}

impl From<CalendarFields> for YearMonthCalendarFields {
    fn from(value: CalendarFields) -> Self {
        Self {
            year: value.year,
            month: value.month,
            month_code: value.month_code,
            era: value.era,
            era_year: value.era_year,
        }
    }
}

#[derive(Debug, Default, Clone, PartialEq)]
pub struct YearMonthCalendarFields {
    // A potentially set `year` field.
    pub year: Option<i32>,
    // A potentially set `month` field.
    pub month: Option<u8>,
    // A potentially set `month_code` field.
    pub month_code: Option<MonthCode>,
    // A potentially set `era` field.
    pub era: Option<TinyAsciiStr<19>>,
    // A potentially set `era_year` field.
    pub era_year: Option<i32>,
}

impl YearMonthCalendarFields {
    pub const fn new() -> Self {
        Self {
            year: None,
            month: None,
            month_code: None,
            era: None,
            era_year: None,
        }
    }

    pub const fn with_era(mut self, era: Option<TinyAsciiStr<19>>) -> Self {
        self.era = era;
        self
    }

    pub const fn with_era_year(mut self, era_year: Option<i32>) -> Self {
        self.era_year = era_year;
        self
    }

    pub const fn with_year(mut self, year: i32) -> Self {
        self.year = Some(year);
        self
    }

    pub const fn with_optional_year(mut self, year: Option<i32>) -> Self {
        self.year = year;
        self
    }

    pub const fn with_month(mut self, month: u8) -> Self {
        self.month = Some(month);
        self
    }

    pub const fn with_optional_month(mut self, month: Option<u8>) -> Self {
        self.month = month;
        self
    }

    pub const fn with_month_code(mut self, month_code: MonthCode) -> Self {
        self.month_code = Some(month_code);
        self
    }

    pub const fn with_optional_month_code(mut self, month_code: Option<MonthCode>) -> Self {
        self.month_code = month_code;
        self
    }
}

impl YearMonthCalendarFields {
    pub(crate) fn try_from_year_month(year_month: &PlainYearMonth) -> TemporalResult<Self> {
        let (year, era, era_year) = if year_month.era().is_some() {
            (
                None,
                year_month
                    .era()
                    .map(|t| TinyAsciiStr::<19>::try_from_utf8(t.as_bytes()))
                    .transpose()
                    .map_err(|_| TemporalError::general("Invalid era"))?,
                year_month.era_year(),
            )
        } else {
            (Some(year_month.year()), None, None)
        };
        Ok(Self {
            year,
            month: Some(year_month.month()),
            month_code: Some(year_month.month_code()),
            era,
            era_year,
        })
    }

    pub fn is_empty(&self) -> bool {
        *self == Self::new()
    }

    crate::impl_with_fallback_method!(with_fallback_year_month, CalendarFields, () PlainYearMonth);
    crate::impl_field_keys_to_ignore!(());
}

// Use macro to impl fallback methods to avoid having a trait method.
#[doc(hidden)]
#[macro_export]
macro_rules! impl_with_fallback_method {
    ($method_name:ident, $fields_type:ident, ( $(with_day: $day:ident)? ) $component_type:ty) => {
        pub(crate) fn $method_name(&self, fallback: &$component_type, calendar: icu_calendar::AnyCalendarKind, overflow: Overflow) -> TemporalResult<Self> {
            let keys_to_ignore = self.field_keys_to_ignore(calendar);
            let mut era = self.era;

            let mut era_year = self.era_year;
            let mut year = self.year;

            if !keys_to_ignore.era {
                if era.is_none() {
                    era =
                        fallback.era().map(|e| {
                            TinyAsciiStr::<19>::try_from_utf8(e.as_bytes())
                                .map_err(|_| TemporalError::assert().with_message("Produced invalid era code"))
                        })
                        .transpose()?
                }
                if era_year.is_none() {
                    era_year = fallback.era_year();
                }
            }
            if !keys_to_ignore.arithmetical_year {
                if year.is_none() {
                    year = Some(fallback.year());
                }
            }

            let (month, month_code) = match (self.month, self.month_code) {
                (Some(month), Some(mc)) => (Some(month), Some(mc)),
                (Some(month), None) => {
                    let month_maybe_clamped = if overflow == Overflow::Constrain {
                        // TODO (manishearth) this should be managed by ICU4X
                        // https://github.com/unicode-org/icu4x/issues/6790
                        month.clamp(1, 12)
                    } else {
                        month
                    };

                    (Some(month_maybe_clamped), Some(month_to_month_code(month_maybe_clamped)?))
                }
                (None, Some(mc)) => (Some(mc.to_month_integer()).map(Into::into), Some(mc)),
                (None, None) if !keys_to_ignore.month => (
                    Some(fallback.month()).map(Into::into),
                    Some(fallback.month_code()),
                ),
                // This should currently be unreachable, but it may change as CalendarFieldKeysToIgnore
                // changes
                (None, None) => (None, None)
            };
            #[allow(clippy::needless_update)] {
                Ok(Self {
                    year,
                    month,
                    month_code,
                    $($day: Some(self.day.unwrap_or(fallback.day().into())),)?
                    era,
                    era_year,
                })
            }
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! impl_field_keys_to_ignore {
    (( $(with_day: $day:ident)? )) => {
        /// <https://tc39.es/proposal-temporal/#sec-temporal-calendarfieldkeystoignore>
        fn field_keys_to_ignore(&self, calendar: icu_calendar::AnyCalendarKind) -> $crate::builtins::core::calendar::fields::FieldKeysToIgnore {
            let mut keys = $crate::builtins::core::calendar::fields::FieldKeysToIgnore::default();
            // All calendars have months/month codes
            if self.month.is_some() || self.month_code.is_some() {
                keys.month = true;
            }
            if Calendar::calendar_has_eras(calendar) {
                // We should clear years only if the calendar has eras
                if self.year.is_some() || self.era_year.is_some() || self.era.is_some() {
                    keys.era = true;
                    keys.arithmetical_year = true;
                }

                // In a calendar such as "japanese" where eras do not start and end at year and/or month boundaries, note that the returned
                // List should contain era and era-year if keys contains day, month, or month-code
                // (not only if it contains era, era-year, or year, as in the example above) because it's possible for
                // changing the day or month to cause a conflict with the era.
                if calendar == icu_calendar::AnyCalendarKind::Japanese {
                    if self.month.is_some() || self.month_code.is_some() {
                        keys.era = true;
                    }

                    $(
                        if self.$day.is_some() {
                            keys.era = true;
                        }
                    )?
                }
            }

            keys
        }
    };
}
