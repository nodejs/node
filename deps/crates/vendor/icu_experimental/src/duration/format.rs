// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{options::*, Duration, DurationFormatter};

use super::validated_options::Unit;
use core::fmt;
use core::fmt::Write;
use either::Either;
use fixed_decimal::{Decimal, SignDisplay, UnsignedDecimal};
use icu_decimal::FormattedDecimal;
use smallvec::SmallVec;
use writeable::{adapters::WithPart, PartsWrite, Writeable};

macro_rules! create_unit_parts {
    ($($part:ident, $unit:expr),*) => {
        $(
            pub const $part: Part = Part {
                category: "unit",
                value: $unit.as_unit_formatter_name(),
            };
        )*
    };
}

pub mod parts {
    // The below constants are not directly used in the library,
    // but are instead generated programmatically by [`super::FormattedDuration::partition_duration_format_pattern`].
    #![allow(dead_code)]

    use crate::duration::validated_options::Unit;
    use writeable::Part;

    create_unit_parts!(
        YEAR,
        Unit::Year,
        MONTH,
        Unit::Month,
        WEEK,
        Unit::Week,
        DAY,
        Unit::Day,
        HOUR,
        Unit::Hour,
        MINUTE,
        Unit::Minute,
        SECOND,
        Unit::Second,
        MILLISECOND,
        Unit::Millisecond,
        MICROSECOND,
        Unit::Microsecond,
        NANOSECOND,
        Unit::Nanosecond
    );

    pub const LITERAL: Part = Part {
        category: "duration",
        value: "literal",
    };
}

/// The [`Writeable`] implementation that is returned by [`DurationFormatter::format`]. See
/// the [`writeable`] crate for how to consume this.
#[derive(Debug)]
pub struct FormattedDuration<'l> {
    pub(crate) fmt: &'l DurationFormatter,
    pub(crate) duration: &'l Duration,
}

/// Exists to allow creating lists of heterogeneous [`Writeable`]s to pass to [`ListFormatter`].
/// The (Unit, Decimal) pair is used to crerate [`FormattedUnit`]s.
type HeterogenousToFormatter = Either<DigitalDuration, (Unit, Decimal)>;

/// Describes a formatted duration.
#[derive(Default)]
struct DigitalDuration {
    hours: Option<Decimal>,
    add_hour_minute_separator: bool,
    minutes: Option<Decimal>,
    add_minute_second_separator: bool,
    seconds: Option<Decimal>,
}

impl DigitalDuration {
    fn format<'l>(&'l self, fmt: &'l DurationFormatter) -> FormattedDigitalDuration<'l> {
        FormattedDigitalDuration {
            fmt,
            hours: self.hours.as_ref().map(|h| fmt.fdf.format(h)),
            add_hour_minute_separator: self.add_hour_minute_separator,
            minutes: self.minutes.as_ref().map(|m| fmt.fdf.format(m)),
            add_minute_second_separator: self.add_minute_second_separator,
            seconds: self.seconds.as_ref().map(|s| fmt.fdf.format(s)),
        }
    }
}

struct FormattedDigitalDuration<'l> {
    fmt: &'l DurationFormatter,
    hours: Option<FormattedDecimal<'l>>,
    add_hour_minute_separator: bool,
    minutes: Option<FormattedDecimal<'l>>,
    add_minute_second_separator: bool,
    seconds: Option<FormattedDecimal<'l>>,
}

impl Writeable for FormattedDigitalDuration<'_> {
    fn write_to_parts<S: PartsWrite + ?Sized>(&self, sink: &mut S) -> fmt::Result {
        if let Some(hours) = &self.hours {
            sink.with_part(parts::HOUR, |w| hours.write_to_parts(w))?;
        }
        if self.add_hour_minute_separator {
            sink.with_part(parts::LITERAL, |w| {
                w.write_str(&self.fmt.digital.get().separator)
            })?;
        }
        if let Some(minutes) = &self.minutes {
            sink.with_part(parts::MINUTE, |w| minutes.write_to_parts(w))?;
        }
        if self.add_minute_second_separator {
            sink.with_part(parts::LITERAL, |w| {
                w.write_str(&self.fmt.digital.get().separator)
            })?;
        }
        if let Some(seconds) = &self.seconds {
            sink.with_part(parts::SECOND, |w| seconds.write_to_parts(w))?;
        }

        Ok(())
    }
}

impl FormattedDuration<'_> {
    /// Section 1.1.9
    /// Formats numeric hours to [`DigitalDurationFormatter`]. Requires hours formatting style to be either `Numeric` or `TwoDigit`.
    fn format_numeric_hours(
        &self,
        formatted_digital_duration: &mut DigitalDuration,
        sign_displayed: &mut bool,
    ) {
        // 1. Let hoursStyle be durationFormat.[[HoursStyle]].

        // 2. Assert: hoursStyle is "numeric" or hoursStyle is "2-digit".
        debug_assert!(
            self.fmt.options.hour == FieldStyle::Numeric
                || self.fmt.options.hour == FieldStyle::TwoDigit
        );
        // 3. Let result be a new empty List.
        // 4. Let nfOpts be OrdinaryObjectCreate(null).
        // 5. Let numberingSystem be durationFormat.[[NumberingSystem]].
        // 6. Perform ! CreateDataPropertyOrThrow(nfOpts, "numberingSystem", numberingSystem).

        let mut fd = Decimal::from(self.duration.hours);

        // Since we construct the Decimal from an unsigned hours value, we need to set the sign manually.
        fd.set_sign(self.duration.get_sign());

        // 7. If hoursStyle is "2-digit", then
        if self.fmt.options.hour == FieldStyle::TwoDigit {
            // a. Perform ! CreateDataPropertyOrThrow(nfOpts, "minimumIntegerDigits", 2𝔽).
            fd.absolute.pad_start(2);
        }

        // 8. If signDisplayed is false, then
        if !*sign_displayed {
            // a. Perform ! CreateDataPropertyOrThrow(nfOpts, "signDisplay", "never").
            fd.apply_sign_display(SignDisplay::Never);
        }

        formatted_digital_duration.hours = Some(fd);
    }

    /// Section 1.1.10
    /// Formats numeric minutes to sink. Requires minutes formatting style to be either `Numeric` or `TwoDigit`.
    fn format_numeric_minutes(
        &self,
        formatted_digital_duration: &mut DigitalDuration,
        sign_displayed: &mut bool,
    ) {
        // 1. Let result be a new empty List.

        // 2. If hoursDisplayed is true, then
        if formatted_digital_duration.hours.is_some() {
            // a. Let separator be durationFormat.[[DigitalFormat]].[[HoursMinutesSeparator]].
            // b. Append the Record { [[Type]]: "literal", [[Value]]: separator, [[Unit]]: empty } to result.
            formatted_digital_duration.add_hour_minute_separator = true;
        }

        // 3. Let minutesStyle be durationFormat.[[MinutesStyle]].
        // 4. Assert: minutesStyle is "numeric" or minutesStyle is "2-digit".
        debug_assert!(
            self.fmt.options.minute == FieldStyle::Numeric
                || self.fmt.options.minute == FieldStyle::TwoDigit
        );

        // 5. Let nfOpts be OrdinaryObjectCreate(null).
        // 6. Let numberingSystem be durationFormat.[[NumberingSystem]].
        // 7. Perform ! CreateDataPropertyOrThrow(nfOpts, "numberingSystem", numberingSystem).
        let mut fd = Decimal::from(self.duration.minutes);

        // Since we construct the Decimal from an unsigned minutes value, we need to set the sign manually.
        fd.set_sign(self.duration.get_sign());

        // 8. If minutesStyle is "2-digit", then
        if self.fmt.options.minute == FieldStyle::TwoDigit {
            // a. Perform ! CreateDataPropertyOrThrow(nfOpts, "minimumIntegerDigits", 2𝔽).
            fd.absolute.pad_start(2);
        }

        // 9. If signDisplayed is false, then
        if !*sign_displayed {
            // a. Perform ! CreateDataPropertyOrThrow(nfOpts, "signDisplay", "never").
            fd.apply_sign_display(SignDisplay::Never);
        }

        // 10. Let nf be ! Construct(%NumberFormat%, « durationFormat.[[Locale]], nfOpts »).
        // 11. Let minutesParts be ! PartitionNumberPattern(nf, minutesValue).
        // 12. For each Record { [[Type]], [[Value]] } part of minutesParts, do
        // a. Append the Record { [[Type]]: part.[[Type]], [[Value]]: part.[[Value]], [[Unit]]: "minute" } to result.

        // 13. Return result.
        formatted_digital_duration.minutes = Some(fd);
    }

    /// Section 1.1.7
    /// Computes the sum of all values in durationFormat units with "fractional" style,
    /// expressed as a fraction of the smallest unit of durationFormat that does not use "fractional" style.
    /// then adds the first non-fractional unit and returns it.
    ///
    /// Divergence from standard: adds the first non-fractional unit as well.
    fn add_fractional_digits(&self) -> Decimal {
        let mut prev_val = self.duration.nanoseconds;
        let mut prev_formatted = Decimal::from(prev_val % 1000);
        for (style, val) in [
            (self.fmt.options.microsecond, self.duration.microseconds),
            (self.fmt.options.millisecond, self.duration.milliseconds),
            (self.fmt.options.second, self.duration.seconds),
        ] {
            if style == FieldStyle::Fractional {
                let val = val + prev_val / 1000;
                prev_formatted.absolute = UnsignedDecimal::from(val % 1000)
                    .concatenated_end(prev_formatted.absolute.multiplied_pow10(-3))
                    .unwrap();

                prev_val = val;
            } else {
                return Decimal::new(
                    prev_formatted.sign,
                    UnsignedDecimal::from(val)
                        .concatenated_end(prev_formatted.absolute.multiplied_pow10(-3))
                        .unwrap(),
                );
            }
        }

        prev_formatted
    }

    /// Section 1.1.11
    /// Formats numeric seconds to sink. Requires seconds formatting style to be either `Numeric` or `TwoDigit`.
    fn format_numeric_seconds(
        &self,
        mut second_fd: Decimal,
        formatted_digital_duration: &mut DigitalDuration,
        sign_displayed: &mut bool,
    ) {
        // 1. Let secondsStyle be durationFormat.[[SecondsStyle]].
        // 2. Assert: secondsStyle is "numeric" or secondsStyle is "2-digit".
        debug_assert!(matches!(
            self.fmt.options.second,
            FieldStyle::Numeric | FieldStyle::TwoDigit
        ));

        // 3. Let result be a new empty List.

        // 4. If minutesDisplayed is true, then
        if formatted_digital_duration.minutes.is_some() {
            // a. Let separator be durationFormat.[[DigitalFormat]].[[MinutesSecondsSeparator]].
            // b. Append the Record { [[Type]]: "literal", [[Value]]: separator, [[Unit]]: empty } to result.
            formatted_digital_duration.add_minute_second_separator = true;
        }

        // Since we construct the Decimal from an unsigned seconds value, we need to set the sign manually.
        second_fd.set_sign(self.duration.get_sign());

        // 5. Let nfOpts be OrdinaryObjectCreate(null).
        // 6. Let numberingSystem be durationFormat.[[NumberingSystem]].
        // 7. Perform ! CreateDataPropertyOrThrow(nfOpts, "numberingSystem", numberingSystem).

        // 8. If secondsStyle is "2-digit", then
        if self.fmt.options.second == FieldStyle::TwoDigit {
            // a. Perform ! CreateDataPropertyOrThrow(nfOpts, "minimumIntegerDigits", 2𝔽).
            second_fd.absolute.pad_start(2);
        }

        // 9. If signDisplayed is false, then
        if !*sign_displayed {
            // a. Perform ! CreateDataPropertyOrThrow(nfOpts, "signDisplay", "never").
            second_fd.apply_sign_display(SignDisplay::Never);
        }

        // 10. Let nf be ! Construct(%NumberFormat%, « durationFormat.[[Locale]], nfOpts »).

        match self.fmt.options.fractional_digits {
            // 11. If durationFormat.[[FractionalDigits]] is undefined, then
            FractionalDigits::ShowAll => {
                // a. Let maximumFractionDigits be 9𝔽.
                // b. Let minimumFractionDigits be +0𝔽.
                second_fd.trunc(-9);
                second_fd.absolute.pad_end(0);
            }
            // 12. Else,
            FractionalDigits::Fixed(i) => {
                let i: i16 = i.into();
                // a. Let maximumFractionDigits be durationFormat.[[FractionalDigits]].
                second_fd.trunc(-i);
                // b. Let minimumFractionDigits be durationFormat.[[FractionalDigits]].
                second_fd.absolute.pad_end(-i);
            } // 13. Perform ! CreateDataPropertyOrThrow(nfOpts, "maximumFractionDigits", maximumFractionDigits).
              // 14. Perform ! CreateDataPropertyOrThrow(nfOpts, "minimumFractionDigits", minimumFractionDigits).
              // 15. Perform ! CreateDataPropertyOrThrow(nfOpts, "roundingMode", "trunc").
        }

        // 16. Let secondsParts be ! PartitionNumberPattern(nf, secondsValue).
        // 17. For each Record { [[Type]], [[Value]] } part of secondsParts, do
        // a. Append the Record { [[Type]]: part.[[Type]], [[Value]]: part.[[Value]], [[Unit]]: "second" } to result
        formatted_digital_duration.seconds = Some(second_fd);

        // 18. Return result.
    }

    /// Section 1.1.12
    /// Formats the parts of duration that use `Numeric` or `TwoDigit` style to sink.
    fn format_numeric_units(
        &self,
        first_numeric_unit: Unit,
        sign_displayed: &mut bool,
    ) -> DigitalDuration {
        // 1. Assert: firstNumericUnit is "hours", "minutes", or "seconds".
        debug_assert!(matches!(
            first_numeric_unit,
            Unit::Hour | Unit::Minute | Unit::Second
        ));

        // 3. Let hoursValue be duration.[[Hours]].
        // 4. Let hoursDisplay be durationFormat.[[HoursDisplay]].
        // 5. Let minutesValue be duration.[[Minutes]].
        // 6. Let minutesDisplay be durationFormat.[[MinutesDisplay]].
        // 7. Let secondsValue be duration.[[Seconds]].

        let seconds_value = // 8. If duration.[[Milliseconds]] is not 0 or duration.[[Microseconds]] is not 0 or duration.[[Nanoseconds]] is not 0, then
			if self.duration.milliseconds != 0
				|| self.duration.microseconds != 0
				|| self.duration.nanoseconds != 0
			{
				// a. Set secondsValue to secondsValue + AddFractionalDigits(durationFormat, duration).
				self.add_fractional_digits()
			} else {
				Decimal::from(self.duration.seconds)
			};

        // 9. Let secondsDisplay be durationFormat.[[SecondsDisplay]].
        // 10. Let hoursFormatted be false.
        let mut hours_formatted = false;

        // 11. If firstNumericUnit is "hours", then
        if first_numeric_unit == Unit::Hour {
            // a. If hoursValue is not 0 or hoursDisplay is not "auto", then
            if self.duration.hours != 0 || self.fmt.options.hour_visibility != FieldDisplay::Auto {
                // i. Set hoursFormatted to true.
                hours_formatted = true;
            }
        }

        // 12. If secondsValue is not 0 or secondsDisplay is not "auto", then
        // a. Let secondsFormatted be true.
        // 13. Else,
        // a. Let secondsFormatted be false.
        let seconds_formatted =
            self.duration.seconds != 0 || self.fmt.options.second_visibility != FieldDisplay::Auto;

        // 14. Let minutesFormatted be false.
        let mut minutes_formatted = false;

        // 15. If firstNumericUnit is "hours" or firstNumericUnit is "minutes", then
        if first_numeric_unit == Unit::Hour || first_numeric_unit == Unit::Minute {
            // a. If hoursFormatted is true and secondsFormatted is true, then
            // b. Else if minutesValue is not 0 or minutesDisplay is not "auto", then
            if (hours_formatted && seconds_formatted)
                || (self.duration.minutes != 0
                    || self.fmt.options.minute_visibility != FieldDisplay::Auto)
            {
                // i. Set minutesFormatted to true.
                minutes_formatted = true;
            }
        }

        let mut formatted_digital_duration = DigitalDuration::default();

        // 16. If hoursFormatted is true, then
        if hours_formatted {
            // a. Append FormatNumericHours(durationFormat, hoursValue, signDisplayed) to numericPartsList.
            self.format_numeric_hours(&mut formatted_digital_duration, sign_displayed);
            // b. Set signDisplayed to false.
            *sign_displayed = false;
        }
        // 17. If minutesFormatted is true, then
        if minutes_formatted {
            // a. Append FormatNumericMinutes(durationFormat, minutesValue, hoursFormatted, signDisplayed) to numericPartsList.
            self.format_numeric_minutes(&mut formatted_digital_duration, sign_displayed);
            // b. Set signDisplayed to false.
            *sign_displayed = false;
        }

        // 18. If secondsFormatted is true, then
        if seconds_formatted {
            // a. Append FormatNumericSeconds(durationFormat, secondsValue, minutesFormatted, signDisplayed) to numericPartsList.
            self.format_numeric_seconds(
                seconds_value,
                &mut formatted_digital_duration,
                sign_displayed,
            );
            // b. Set signDisplayed to false.
            *sign_displayed = false;
        }

        // 19. Return numericPartsList.
        formatted_digital_duration
    }

    /// Section 1.1.8
    /// Returns true if the next smallest unit uses the "fractional" style.
    fn next_unit_fractional(&self, unit: Unit) -> bool {
        // 1. Assert: unit is "seconds", "milliseconds", or "microseconds".
        debug_assert!(matches!(
            unit,
            Unit::Second | Unit::Millisecond | Unit::Microsecond
        ));

        match unit {
            // 2. If unit is "seconds" and durationFormat.[[MillisecondsStyle]] is "fractional", return true.
            Unit::Second if self.fmt.options.millisecond == FieldStyle::Fractional => true,
            // 3. Else if unit is "milliseconds" and durationFormat.[[MicrosecondsStyle]] is "fractional", return true.
            Unit::Millisecond if self.fmt.options.microsecond == FieldStyle::Fractional => true,
            // 4. Else if unit is "microseconds" and durationFormat.[[NanosecondsStyle]] is "fractional", return true.
            Unit::Microsecond if self.fmt.options.nanosecond == FieldStyle::Fractional => true,
            // 5. Return false.
            _ => false,
        }
    }

    fn partition_duration_format_pattern<V: PartsWrite + ?Sized>(
        &self,
        sink: &mut V,
    ) -> fmt::Result {
        // We can have a maximum of 10 writeables (one FormattedUnit for each unit).
        // In the digital case, one or more unit is represented by the FormattedDigitalDuration,
        // which is a single writeable.
        let mut parts_list: SmallVec<[HeterogenousToFormatter; 10]> = SmallVec::new();

        // 2. Let signDisplayed be true.
        let mut sign_displayed = true;
        // 3. Let numericUnitFound be false.
        let mut numeric_unit_found = false;

        // 4.  While numericUnitFound is false, repeat for each row in Table 2 in table order, except the header row:
        for ((unit, style, display), value) in self
            .fmt
            .options
            .iter_units()
            .into_iter()
            .zip(self.duration.iter_units())
        {
            if numeric_unit_found {
                break;
            }
            // a. Let value be the value of duration's field whose name is the Value Field value of the current row.
            // b. Let style be the value of durationFormat's internal slot whose name is the Style Slot value of the current row.
            // c. Let display be the value of durationFormat's internal slot whose name is the Display Slot value of the current row.
            // d. Let unit be the Unit value of the current row.
            // e. If style is "numeric" or "2-digit", then
            if style == FieldStyle::Numeric || style == FieldStyle::TwoDigit {
                // i. Append FormatNumericUnits(durationFormat, duration, unit, signDisplayed) to result.
                let digital_duration_formatter =
                    self.format_numeric_units(unit, &mut sign_displayed);
                parts_list.push(HeterogenousToFormatter::Left(digital_duration_formatter));
                // ii. Set numericUnitFound to true.
                numeric_unit_found = true;
            }
            // f. Else,
            else {
                let mut formatted_value = Decimal::from(value);
                formatted_value.set_sign(self.duration.get_sign());

                // i. Let nfOpts be OrdinaryObjectCreate(null).
                // ii. If unit is "seconds", "milliseconds", or "microseconds", then
                if matches!(unit, Unit::Second | Unit::Millisecond | Unit::Microsecond) {
                    // 1. If NextUnitFractional(durationFormat, unit) is true, then
                    if self.next_unit_fractional(unit) {
                        // a. Set value to value + AddFractionalDigits(durationFormat, duration).
                        formatted_value = self.add_fractional_digits();
                        match self.fmt.options.fractional_digits {
                            // b. If durationFormat.[[FractionalDigits]] is undefined, then
                            FractionalDigits::ShowAll => {
                                // i. Let maximumFractionDigits be 9𝔽.
                                formatted_value.trunc(-9);
                                // ii. Let minimumFractionDigits be +0𝔽.
                                formatted_value.absolute.pad_end(0);
                            }
                            // c. Else,
                            FractionalDigits::Fixed(i) => {
                                let i: i16 = i.into();
                                // i. Let maximumFractionDigits be durationFormat.[[FractionalDigits]].
                                formatted_value.trunc(-i);
                                // ii. Let minimumFractionDigits be durationFormat.[[FractionalDigits]].
                                formatted_value.absolute.pad_end(-i);
                            }
                        } // d. Perform ! CreateDataPropertyOrThrow(nfOpts, "maximumFractionDigits", maximumFractionDigits).
                          // e. Perform ! CreateDataPropertyOrThrow(nfOpts, "minimumFractionDigits", minimumFractionDigits).
                          // f. Perform ! CreateDataPropertyOrThrow(nfOpts, "roundingMode", "trunc").

                        // g. Set numericUnitFound to true.
                        numeric_unit_found = true;
                    }
                }
                //  iii. If value is not 0 or display is not "auto", then
                if value != 0 || display != FieldDisplay::Auto {
                    // 1. Let numberingSystem be durationFormat.[[NumberingSystem]].
                    // 2. Perform ! CreateDataPropertyOrThrow(nfOpts, "numberingSystem", numberingSystem).
                    // 3. If signDisplayed is true, then
                    if sign_displayed {
                        // a. Set signDisplayed to false.
                        sign_displayed = false;
                        // b. If value is 0 and DurationSign(duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], duration.[[Hours]], duration.[[Minutes]], duration.[[Seconds]], duration.[[Milliseconds]], duration.[[Microseconds]], duration.[[Nanoseconds]]) is -1, then
                        if value == 0 && self.duration.get_sign() == fixed_decimal::Sign::Negative {
                            // i. Set value to negative-zero.
                            formatted_value.apply_sign_display(SignDisplay::Always);
                        }
                    }
                    // 4. Else,
                    else {
                        // a. Perform ! CreateDataPropertyOrThrow(nfOpts, "signDisplay", "never").
                        formatted_value.apply_sign_display(SignDisplay::Never);
                    }

                    // 5. Let numberFormatUnit be the NumberFormat Unit value of the current row.
                    // 6. Perform ! CreateDataPropertyOrThrow(nfOpts, "style", "unit").
                    // 7. Perform ! CreateDataPropertyOrThrow(nfOpts, "unit", numberFormatUnit).
                    // 8. Perform ! CreateDataPropertyOrThrow(nfOpts, "unitDisplay", style).
                    // NOTE: we perform the above steps while initializing DurationFormatter.
                    // 9. Let nf be ! Construct(%NumberFormat%, « durationFormat.[[Locale]], nfOpts »).

                    // 10. Let parts be ! PartitionNumberPattern(nf, value).

                    // 11. Let list be a new empty List.
                    // 12. For each Record { [[Type]], [[Value]] } part of parts, do
                    // a. Append the Record { [[Type]]: part.[[Type]], [[Value]]: part.[[Value]], [[Unit]]: numberFormatUnit } to list.
                    // 13. Append list to result.
                    parts_list.push(HeterogenousToFormatter::Right((unit, formatted_value)));
                }
            }
        }

        // 5. Return ListFormatParts(durationFormat, result).
        self.list_format_parts(parts_list, sink)
    }

    /// 1.1.13 `ListFormatParts` ( durationFormat, partitionedPartsList )
    /// Given a partitioned part list of formatted duration parts, it creates and returns a List with all the corresponding parts according to the effective locale and the formatting options of durationFormat.
    fn list_format_parts<V: PartsWrite + ?Sized, const CAP: usize>(
        &self,
        parts_list: SmallVec<[HeterogenousToFormatter; CAP]>,
        sink: &mut V,
    ) -> fmt::Result {
        // 1. Let lfOpts be OrdinaryObjectCreate(null).
        // 2. Perform ! CreateDataPropertyOrThrow(lfOpts, "type", "unit").
        // 3. Let listStyle be durationFormat.[[Style]].

        // 4. If listStyle is "digital", then
        // a. Set listStyle to "short".

        // 5. Perform ! CreateDataPropertyOrThrow(lfOpts, "style", listStyle).
        // 6. Let lf be ! Construct(%ListFormat%, « durationFormat.[[Locale]], lfOpts »).

        // Note: the above steps are performed while initializing DurationFormatter.
        let formatted_list = self.fmt.list.format(parts_list.iter().map(|w| match w {
            Either::Left(digital) => Either::Left(digital.format(self.fmt)),
            Either::Right((unit, formatted_fixed_decimal)) => {
                let nf = &self.fmt.unit[*unit];

                let formatted_unit = WithPart {
                    part: writeable::Part {
                        category: "unit",
                        value: unit.as_unit_formatter_name(),
                    },
                    writeable: nf.format_fixed_decimal(formatted_fixed_decimal),
                };
                Either::Right(formatted_unit)
            }
        }));

        formatted_list.write_to_parts(sink)?;

        // 14. Return flattenedPartsList.
        Ok(())
    }
}

impl Writeable for FormattedDuration<'_> {
    fn write_to_parts<S: PartsWrite + ?Sized>(&self, sink: &mut S) -> fmt::Result {
        self.partition_duration_format_pattern(sink)
    }
}

writeable::impl_display_with_writeable!(FormattedDuration<'_>);

#[cfg(test)]
mod tests {
    use icu_locale::locale;
    use writeable::assert_writeable_parts_eq;

    use super::*;
    use crate::duration::{formatter::ValidatedDurationFormatterOptions, DurationSign};

    #[test]
    fn test_digital_formatter() {
        let duration = Duration {
            sign: DurationSign::Positive,
            years: 0,
            months: 0,
            weeks: 0,
            days: 0,
            hours: 12,
            minutes: 1,
            seconds: 32,
            milliseconds: 130,
            microseconds: 0,
            nanoseconds: 0,
        };

        let options = DurationFormatterOptions {
            base: BaseStyle::Digital,
            ..Default::default()
        };

        let options = ValidatedDurationFormatterOptions::validate(options).unwrap();
        let formatter = DurationFormatter::try_new(locale!("und").into(), options).unwrap();
        let formatted = formatter.format(&duration);
        assert_eq!(formatted.write_to_string().into_owned(), "12:01:32.13");
    }

    #[test]
    fn test_duration_formatter() {
        let duration = Duration {
            sign: DurationSign::Negative,
            years: 0, // test always-display leading zero
            months: 2,
            weeks: 3,
            hours: 12,
            minutes: 1,
            seconds: 5,
            milliseconds: 130,
            microseconds: 140,
            ..Default::default()
        };

        let options = DurationFormatterOptions {
            base: BaseStyle::Long,
            year_visibility: Some(FieldDisplay::Always),
            millisecond: Some(MilliSecondStyle::Numeric),
            ..Default::default()
        };

        let options = ValidatedDurationFormatterOptions::validate(options).unwrap();
        let formatter = DurationFormatter::try_new(locale!("en").into(), options).unwrap();
        let formatted = formatter.format(&duration);
        assert_eq!(
            formatted.write_to_string().into_owned(),
            "-0 years, 2 months, 3 weeks, 12 hours, 1 minute, 5.13014 seconds"
        );
    }

    #[test]
    fn test_duration_formatter_parts() {
        let duration = Duration {
            sign: DurationSign::Negative,
            years: 0,
            months: 2,
            weeks: 3,
            hours: 12,
            minutes: 1,
            seconds: 5,
            milliseconds: 130,
            microseconds: 140,
            ..Default::default()
        };

        let options = DurationFormatterOptions {
            base: BaseStyle::Digital,
            year_visibility: Some(FieldDisplay::Always),
            ..Default::default()
        };

        let options = ValidatedDurationFormatterOptions::validate(options).unwrap();
        let formatter = DurationFormatter::try_new(locale!("en").into(), options).unwrap();
        let formatted = formatter.format(&duration);
        assert_writeable_parts_eq!(
            &formatted,
            "-0 yrs, 2 mths, 3 wks, 12:01:05.13014",
            [
                (0, 6, parts::YEAR),
                (0, 6, icu_list::parts::ELEMENT),
                (0, 1, icu_decimal::parts::MINUS_SIGN),
                (1, 2, icu_decimal::parts::INTEGER),
                (6, 8, icu_list::parts::LITERAL),
                (8, 14, parts::MONTH),
                (8, 14, icu_list::parts::ELEMENT),
                (8, 9, icu_decimal::parts::INTEGER),
                (14, 16, icu_list::parts::LITERAL),
                (16, 21, parts::WEEK),
                (16, 21, icu_list::parts::ELEMENT),
                (16, 17, icu_decimal::parts::INTEGER),
                (21, 23, icu_list::parts::LITERAL),
                (23, 37, icu_list::parts::ELEMENT),
                (23, 25, icu_decimal::parts::INTEGER),
                (23, 25, parts::HOUR),
                (25, 26, parts::LITERAL),
                (26, 28, icu_decimal::parts::INTEGER),
                (26, 28, parts::MINUTE),
                (28, 29, parts::LITERAL),
                (29, 37, parts::SECOND),
                (29, 31, icu_decimal::parts::INTEGER),
                (31, 32, icu_decimal::parts::DECIMAL),
                (32, 37, icu_decimal::parts::FRACTION),
            ]
        );
    }

    #[test]
    fn test_positive_negative_zero() {
        let positive_duration = Duration {
            sign: DurationSign::Positive,
            ..Default::default()
        };

        let negative_duration = Duration {
            sign: DurationSign::Negative,
            ..Default::default()
        };

        let options = DurationFormatterOptions {
            year_visibility: Some(FieldDisplay::Always),
            ..Default::default()
        };

        let options = ValidatedDurationFormatterOptions::validate(options).unwrap();
        let formatter = DurationFormatter::try_new(locale!("en").into(), options).unwrap();

        assert_eq!(
            formatter
                .format(&positive_duration)
                .write_to_string()
                .into_owned(),
            "0 yrs"
        );
        assert_eq!(
            formatter
                .format(&negative_duration)
                .write_to_string()
                .into_owned(),
            "0 yrs"
        );

        let negative_non_zero_duration = Duration {
            sign: DurationSign::Negative,
            years: 0,
            months: 1,
            ..Default::default()
        };

        assert_eq!(
            formatter
                .format(&negative_non_zero_duration)
                .write_to_string()
                .into_owned(),
            "-0 yrs, 1 mth"
        );
    }
}
