// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::duration::options::*;

/// Validated options for [`DurationFormatter`](crate::duration::DurationFormatter).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ValidatedDurationFormatterOptions {
    /// The style that will be applied to units
    /// unless overridden by a specific style.
    pub(crate) base: BaseStyle,

    /// Style for year
    pub(crate) year: FieldStyle,
    /// Visibility control for year
    pub(crate) year_visibility: FieldDisplay,
    /// Style for month
    pub(crate) month: FieldStyle,
    /// Visibility control for month
    pub(crate) month_visibility: FieldDisplay,
    /// Style for week
    pub(crate) week: FieldStyle,
    /// Visibility control for week
    pub(crate) week_visibility: FieldDisplay,
    /// Style for day
    pub(crate) day: FieldStyle,
    /// Visibility control for day
    pub(crate) day_visibility: FieldDisplay,
    /// Style for hour
    pub(crate) hour: FieldStyle,
    /// Visibility control for hour
    pub(crate) hour_visibility: FieldDisplay,
    /// Style for minute
    pub(crate) minute: FieldStyle,
    /// Visibility control for minute
    pub(crate) minute_visibility: FieldDisplay,
    /// Style for second
    pub(crate) second: FieldStyle,
    /// Visibility control for second
    pub(crate) second_visibility: FieldDisplay,
    /// Style for millisecond
    pub(crate) millisecond: FieldStyle,
    /// Visibility control for millisecond
    pub(crate) millisecond_visibility: FieldDisplay,
    /// Style for microsecond
    pub(crate) microsecond: FieldStyle,
    /// Visibility control for microsecond
    pub(crate) microsecond_visibility: FieldDisplay,
    /// Style for nanosecond
    pub(crate) nanosecond: FieldStyle,
    /// Visibility control for nanosecond
    pub(crate) nanosecond_visibility: FieldDisplay,

    /// Number of fractional digits to use when formatting sub-second units (milliseconds, microseconds, nanoseconds).
    /// ### Note:
    /// - Only takes effect when the subsecond units are styled as `Numeric`.
    /// - Zero means no fractional digits.
    pub(crate) fractional_digits: FractionalDigits,
}

/// Error type for [`DurationFormatterOptions`] validation.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, displaydoc::Display)]
pub enum DurationFormatterOptionsError {
    /// A unit field is set to [`FieldDisplay::Always`] and the style is set to `Fractional`.
    #[displaydoc("A unit field is set to Always and the style is set to Fractional")]
    DisplayAlwaysFractional,

    /// A previous unit's style is `Fractional`, but the following unit's style is not `Fractional`.
    #[displaydoc(
        "A previous unit's style is Fractional, but the following unit's style is not Fractional"
    )]
    PreviousFractional,

    /// A previous unit's style is set to `Numeric` or `TwoDigit` and the following unit's style is not
    /// `Fractional`, `Numeric`, or `TwoDigit`.
    #[displaydoc("A previous unit's style is set to Numeric or TwoDigit and the following unit's style is not Fractional, Numeric, or TwoDigit")]
    PreviousNumeric,

    /// The number of fractional digits is out of acceptable range. See [`FractionalDigits::Fixed`].
    #[displaydoc("The number of fractional digits is out of acceptable range")]
    FractionalDigitsOutOfRange,
}

impl core::error::Error for DurationFormatterOptionsError {}

impl ValidatedDurationFormatterOptions {
    /// Constructs a [`ValidatedDurationFormatterOptions`] from an unvalidated
    /// [`DurationFormatterOptions`] by checking for invariants and resolving various implied
    /// field values.
    ///
    /// Follows <https://tc39.es/proposal-intl-duration-format/#sec-getdurationunitoptions>
    pub fn validate(
        value: DurationFormatterOptions,
    ) -> Result<Self, DurationFormatterOptionsError> {
        let mut builder: ValidatedDurationFormatterOptionsBuilder = value.into();

        let units = builder.iter_units();

        // section 1.1.6
        let mut prev_style = None;
        for (unit, style, visibility) in units.into_iter() {
            // 2. Let displayDefault be "always".
            let mut default_visibility = FieldDisplay::Always;

            // 3. If style is undefined, then
            if style.is_none() {
                // a. If baseStyle is "digital", then
                if value.base == BaseStyle::Digital {
                    // i. If unit is not one of "hours", "minutes", or "seconds", then
                    if unit != Unit::Hour || unit != Unit::Minute || unit != Unit::Second {
                        // 1. Set displayDefault to "auto".
                        default_visibility = FieldDisplay::Auto;
                    }
                    // ii. Set style to digitalBase.
                    *style = Some(unit.digital_default());
                }
                // b. Else,
                else {
                    // i. If prevStyle is "fractional", "numeric" or "2-digit", then
                    if matches!(
                        prev_style,
                        Some(FieldStyle::Fractional | FieldStyle::Numeric | FieldStyle::TwoDigit)
                    ) {
                        // 1. If unit is not one of "minutes" or "seconds", then
                        if unit != Unit::Minute || unit != Unit::Second {
                            // a. Set displayDefault to "auto".
                            default_visibility = FieldDisplay::Auto;
                        }
                        // 2. Set style to "numeric".
                        *style = Some(FieldStyle::Numeric);
                    }
                    // ii. Else,
                    else {
                        // 1. Set displayDefault to "auto".
                        default_visibility = FieldDisplay::Auto;
                        // 2. Set style to baseStyle.
                        *style = Some(value.base.into());
                    }
                }
            }

            // 4. If style is "numeric", then
            if *style == Some(FieldStyle::Numeric) {
                // a. If unit is one of "milliseconds", "microseconds", or "nanoseconds", then
                if unit == Unit::Millisecond
                    || unit == Unit::Microsecond
                    || unit == Unit::Nanosecond
                {
                    // i. Set style to "fractional".
                    *style = Some(FieldStyle::Fractional);
                    // ii. Set displayDefault to "auto".
                    default_visibility = FieldDisplay::Auto;
                }
            }

            // 5. Let displayField be the string-concatenation of unit and "Display".
            // 6. Let display be ? GetOption(options, displayField, string, « "auto", "always" », displayDefault).
            if visibility.is_none() {
                *visibility = Some(default_visibility);
            }

            // 7. If display is "always" and style is "fractional", then
            if *visibility == Some(FieldDisplay::Always) && *style == Some(FieldStyle::Fractional) {
                // a. Throw a RangeError exception.
                return Err(DurationFormatterOptionsError::DisplayAlwaysFractional);
            }

            // 8. If prevStyle is "fractional", then
            if prev_style == Some(FieldStyle::Fractional) {
                // a. If style is not "fractional", then
                if *style != Some(FieldStyle::Fractional) {
                    // i. Throw a RangeError exception.
                    return Err(DurationFormatterOptionsError::PreviousFractional);
                }
            }

            // 9. If prevStyle is "numeric" or "2-digit", then
            if prev_style == Some(FieldStyle::Numeric) || prev_style == Some(FieldStyle::TwoDigit) {
                // a. If style is not "fractional", "numeric" or "2-digit", then
                if !matches!(
                    *style,
                    Some(FieldStyle::Fractional | FieldStyle::Numeric | FieldStyle::TwoDigit)
                ) {
                    // i. Throw a RangeError exception.
                    return Err(DurationFormatterOptionsError::PreviousNumeric);
                }
                // b. If unit is "minutes" or "seconds", then
                if unit == Unit::Minute || unit == Unit::Second {
                    // i. Set style to "2-digit".
                    *style = Some(FieldStyle::TwoDigit);
                }
            }

            // 10. If unit is "hours" and twoDigitHours is true, then
            // NOTE:
            // Divergence from TC39 standard:
            // We do not check for twoDigitHours here.
            // We later check whether we have to display hours, and then use leading separator padding
            // for displaying hours with two digits.

            prev_style = *style;
        }

        // section 1.2.1
        // 27. Set durationFormat.[[FractionalDigits]] to ? GetNumberOption(options, "fractionalDigits", 0, 9, undefined).
        if let FractionalDigits::Fixed(i) = builder.fractional_digits {
            if i > 9 {
                return Err(DurationFormatterOptionsError::FractionalDigitsOutOfRange);
            }
        }

        Ok(builder.try_into().unwrap())
    }

    /// Iterates over all unit fields of the struct, returning a tuple of the unit,
    /// and mutable references to its style and the visibility.
    /// See also: [`iter_units`](ValidatedDurationFormatterOptions::iter_units).
    #[allow(dead_code)]
    pub(crate) fn iter_mut_units(&mut self) -> [(Unit, &mut FieldStyle, &mut FieldDisplay); 10] {
        [
            (Unit::Year, &mut self.year, &mut self.year_visibility),
            (Unit::Month, &mut self.month, &mut self.month_visibility),
            (Unit::Week, &mut self.week, &mut self.week_visibility),
            (Unit::Day, &mut self.day, &mut self.day_visibility),
            (Unit::Hour, &mut self.hour, &mut self.hour_visibility),
            (Unit::Minute, &mut self.minute, &mut self.minute_visibility),
            (Unit::Second, &mut self.second, &mut self.second_visibility),
            (
                Unit::Millisecond,
                &mut self.millisecond,
                &mut self.millisecond_visibility,
            ),
            (
                Unit::Microsecond,
                &mut self.microsecond,
                &mut self.microsecond_visibility,
            ),
            (
                Unit::Nanosecond,
                &mut self.nanosecond,
                &mut self.nanosecond_visibility,
            ),
        ]
    }

    /// Iterates over all unit fields of the struct, returning a tuple of the unit,
    /// and references to its style and the visibility.
    /// See also: [`iter_mut_units`](ValidatedDurationFormatterOptions::iter_mut_units).
    pub(crate) fn iter_units(&self) -> [(Unit, FieldStyle, FieldDisplay); 10] {
        [
            (Unit::Year, self.year, self.year_visibility),
            (Unit::Month, self.month, self.month_visibility),
            (Unit::Week, self.week, self.week_visibility),
            (Unit::Day, self.day, self.day_visibility),
            (Unit::Hour, self.hour, self.hour_visibility),
            (Unit::Minute, self.minute, self.minute_visibility),
            (Unit::Second, self.second, self.second_visibility),
            (
                Unit::Millisecond,
                self.millisecond,
                self.millisecond_visibility,
            ),
            (
                Unit::Microsecond,
                self.microsecond,
                self.microsecond_visibility,
            ),
            (
                Unit::Nanosecond,
                self.nanosecond,
                self.nanosecond_visibility,
            ),
        ]
    }
}

/// Validated options builder for [DurationFormatter](DurationFormatter).
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
struct ValidatedDurationFormatterOptionsBuilder {
    base: BaseStyle,

    year: Option<FieldStyle>,
    year_visibility: Option<FieldDisplay>,
    month: Option<FieldStyle>,
    month_visibility: Option<FieldDisplay>,
    week: Option<FieldStyle>,
    week_visibility: Option<FieldDisplay>,
    day: Option<FieldStyle>,
    day_visibility: Option<FieldDisplay>,
    hour: Option<FieldStyle>,
    hour_visibility: Option<FieldDisplay>,
    minute: Option<FieldStyle>,
    minute_visibility: Option<FieldDisplay>,
    second: Option<FieldStyle>,
    second_visibility: Option<FieldDisplay>,
    millisecond: Option<FieldStyle>,
    millisecond_visibility: Option<FieldDisplay>,
    microsecond: Option<FieldStyle>,
    microsecond_visibility: Option<FieldDisplay>,
    nanosecond: Option<FieldStyle>,
    nanosecond_visibility: Option<FieldDisplay>,
    fractional_digits: FractionalDigits,
}

impl ValidatedDurationFormatterOptionsBuilder {
    fn iter_units(&mut self) -> [(Unit, &mut Option<FieldStyle>, &mut Option<FieldDisplay>); 10] {
        [
            (Unit::Year, &mut self.year, &mut self.year_visibility),
            (Unit::Month, &mut self.month, &mut self.month_visibility),
            (Unit::Week, &mut self.week, &mut self.week_visibility),
            (Unit::Day, &mut self.day, &mut self.day_visibility),
            (Unit::Hour, &mut self.hour, &mut self.hour_visibility),
            (Unit::Minute, &mut self.minute, &mut self.minute_visibility),
            (Unit::Second, &mut self.second, &mut self.second_visibility),
            (
                Unit::Millisecond,
                &mut self.millisecond,
                &mut self.millisecond_visibility,
            ),
            (
                Unit::Microsecond,
                &mut self.microsecond,
                &mut self.microsecond_visibility,
            ),
            (
                Unit::Nanosecond,
                &mut self.nanosecond,
                &mut self.nanosecond_visibility,
            ),
        ]
    }
}

impl From<DurationFormatterOptions> for ValidatedDurationFormatterOptionsBuilder {
    fn from(value: DurationFormatterOptions) -> Self {
        ValidatedDurationFormatterOptionsBuilder {
            base: value.base,
            year: value.year.map(FieldStyle::from),
            year_visibility: value.year_visibility,
            month: value.month.map(FieldStyle::from),
            month_visibility: value.month_visibility,
            week: value.week.map(FieldStyle::from),
            week_visibility: value.week_visibility,
            day: value.day.map(FieldStyle::from),
            day_visibility: value.day_visibility,
            hour: value.hour.map(FieldStyle::from),
            hour_visibility: value.hour_visibility,
            minute: value.minute.map(FieldStyle::from),
            minute_visibility: value.minute_visibility,
            second: value.second.map(FieldStyle::from),
            second_visibility: value.second_visibility,
            millisecond: value.millisecond.map(FieldStyle::from),
            millisecond_visibility: value.millisecond_visibility,
            microsecond: value.microsecond.map(FieldStyle::from),
            microsecond_visibility: value.microsecond_visibility,
            nanosecond: value.nanosecond.map(FieldStyle::from),
            nanosecond_visibility: value.nanosecond_visibility,
            fractional_digits: value.fractional_digits,
        }
    }
}

impl TryFrom<ValidatedDurationFormatterOptionsBuilder> for ValidatedDurationFormatterOptions {
    type Error = ();

    fn try_from(value: ValidatedDurationFormatterOptionsBuilder) -> Result<Self, Self::Error> {
        Ok(ValidatedDurationFormatterOptions {
            base: value.base,
            year: value.year.ok_or(())?,
            year_visibility: value.year_visibility.ok_or(())?,
            month: value.month.ok_or(())?,
            month_visibility: value.month_visibility.ok_or(())?,
            week: value.week.ok_or(())?,
            week_visibility: value.week_visibility.ok_or(())?,
            day: value.day.ok_or(())?,
            day_visibility: value.day_visibility.ok_or(())?,
            hour: value.hour.ok_or(())?,
            hour_visibility: value.hour_visibility.ok_or(())?,
            minute: value.minute.ok_or(())?,
            minute_visibility: value.minute_visibility.ok_or(())?,
            second: value.second.ok_or(())?,
            second_visibility: value.second_visibility.ok_or(())?,
            millisecond: value.millisecond.ok_or(())?,
            millisecond_visibility: value.millisecond_visibility.ok_or(())?,
            microsecond: value.microsecond.ok_or(())?,
            microsecond_visibility: value.microsecond_visibility.ok_or(())?,
            nanosecond: value.nanosecond.ok_or(())?,
            nanosecond_visibility: value.nanosecond_visibility.ok_or(())?,
            fractional_digits: value.fractional_digits,
        })
    }
}

/// An enum to specify the unit being used. Used with [`FieldStyle`] and [`FieldDisplay`] to indicate the field unit.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum Unit {
    Year,
    Month,
    Week,
    Day,
    Hour,
    Minute,
    Second,
    Millisecond,
    Microsecond,
    Nanosecond,
}

impl Unit {
    /// Returns the default digital style for the unit.
    pub(crate) fn digital_default(self) -> FieldStyle {
        match self {
            Unit::Year => YearStyle::Short.into(),
            Unit::Month => MonthStyle::Short.into(),
            Unit::Week => WeekStyle::Short.into(),
            Unit::Day => DayStyle::Short.into(),
            Unit::Hour => HourStyle::Numeric.into(),
            Unit::Minute => MinuteStyle::Numeric.into(),
            Unit::Second => SecondStyle::Numeric.into(),
            Unit::Millisecond => MilliSecondStyle::Numeric.into(),
            Unit::Microsecond => MicroSecondStyle::Numeric.into(),
            Unit::Nanosecond => NanoSecondStyle::Numeric.into(),
        }
    }

    pub(crate) const fn as_unit_formatter_name(self) -> &'static str {
        match self {
            Unit::Year => "year",
            Unit::Month => "month",
            Unit::Week => "week",
            Unit::Day => "day",
            Unit::Hour => "hour",
            Unit::Minute => "minute",
            Unit::Second => "second",
            Unit::Millisecond => "millisecond",
            Unit::Microsecond => "microsecond",
            Unit::Nanosecond => "nanosecond",
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fractional_digit_error() {
        let options = DurationFormatterOptions {
            fractional_digits: FractionalDigits::Fixed(10),
            ..Default::default()
        };

        assert_eq!(
            ValidatedDurationFormatterOptions::validate(options),
            Err(DurationFormatterOptionsError::FractionalDigitsOutOfRange)
        );
    }

    #[test]
    fn test_invalid_style_after_two_digit() {
        let options = DurationFormatterOptions {
            hour: Some(HourStyle::TwoDigit),
            minute: Some(MinuteStyle::Long),
            ..Default::default()
        };

        assert_eq!(
            ValidatedDurationFormatterOptions::validate(options),
            Err(DurationFormatterOptionsError::PreviousNumeric)
        );

        let options = DurationFormatterOptions {
            hour: Some(HourStyle::TwoDigit),
            ..options
        };

        assert_eq!(
            ValidatedDurationFormatterOptions::validate(options),
            Err(DurationFormatterOptionsError::PreviousNumeric)
        );
    }

    #[test]
    fn test_display_always_fractional_style() {
        let options = DurationFormatterOptions {
            millisecond: Some(MilliSecondStyle::Numeric),
            millisecond_visibility: Some(FieldDisplay::Always),
            ..Default::default()
        };

        assert_eq!(
            ValidatedDurationFormatterOptions::validate(options),
            Err(DurationFormatterOptionsError::DisplayAlwaysFractional)
        );
    }

    #[test]
    fn test_previous_fractional() {
        let options = DurationFormatterOptions {
            millisecond: Some(MilliSecondStyle::Numeric),
            nanosecond: Some(NanoSecondStyle::Long),
            ..Default::default()
        };

        assert_eq!(
            ValidatedDurationFormatterOptions::validate(options),
            Err(DurationFormatterOptionsError::PreviousFractional)
        );
    }
}
