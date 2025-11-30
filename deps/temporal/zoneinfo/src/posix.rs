use crate::{
    rule::{LastRules, Rule},
    types::{DayOfMonth, Month, QualifiedTime, Sign, Time, WeekDay},
    utils::month_to_day,
    zone::ZoneEntry,
};
use alloc::string::String;
use core::fmt::Write;

/// The POSIX time zone designated by the [GNU documentation][gnu-docs]
///
/// [gnu-docs]: https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
#[derive(Debug, PartialEq)]
pub struct PosixTimeZone {
    pub abbr: PosixAbbreviation,
    pub offset: Time,
    pub transition_info: Option<PosixTransition>,
}

impl PosixTimeZone {
    pub(crate) fn from_zone_and_savings(entry: &ZoneEntry, savings: Time) -> Self {
        let offset = entry.std_offset.add(savings);
        let formatted = entry
            .format
            .format(offset.as_secs(), None, savings != Time::default());
        let is_numeric = is_numeric(&formatted);
        let abbr = PosixAbbreviation {
            is_numeric,
            formatted,
        };
        Self {
            abbr,
            offset,
            transition_info: None,
        }
    }

    pub(crate) fn from_zone_and_rules(entry: &ZoneEntry, rules: &LastRules) -> Self {
        let offset = entry.std_offset.add(rules.standard.save);
        let formatted = entry.format.format(
            entry.std_offset.as_secs(),
            rules.standard.letter.as_deref(),
            rules.standard.is_dst(),
        );
        let is_numeric = is_numeric(&formatted);
        let abbr = PosixAbbreviation {
            is_numeric,
            formatted,
        };

        let transition_info = rules.saving.as_ref().map(|rule| {
            let formatted = entry.format.format(
                entry.std_offset.as_secs() + rule.save.as_secs(),
                rule.letter.as_deref(),
                rule.is_dst(),
            );
            let abbr = PosixAbbreviation {
                is_numeric,
                formatted,
            };
            let savings = rule.save;
            let start = PosixDateTime::from_rule_and_transition_info(
                rule,
                entry.std_offset,
                rules.standard.save,
            );
            let end = PosixDateTime::from_rule_and_transition_info(
                &rules.standard,
                entry.std_offset,
                rule.save,
            );
            PosixTransition {
                abbr,
                savings,
                start,
                end,
            }
        });

        PosixTimeZone {
            abbr,
            offset,
            transition_info,
        }
    }
}

impl PosixTimeZone {
    pub fn to_string(&self) -> Result<String, core::fmt::Error> {
        let mut posix_string = String::new();
        write_abbr(&self.abbr, &mut posix_string)?;
        write_inverted_time(&self.offset, &mut posix_string)?;

        if let Some(transition_info) = &self.transition_info {
            write_abbr(&transition_info.abbr, &mut posix_string)?;
            if transition_info.savings != Time::one_hour() {
                write_inverted_time(&self.offset.add(transition_info.savings), &mut posix_string)?;
            }
            write_date_time(&transition_info.start, &mut posix_string)?;
            write_date_time(&transition_info.end, &mut posix_string)?;
        }
        Ok(posix_string)
    }
}

/// The representation of a POSIX time zone transition
#[non_exhaustive]
#[derive(Debug, PartialEq)]
pub struct PosixTransition {
    /// The transitions designated abbreviation
    pub abbr: PosixAbbreviation,
    /// The savings value to be added to the offset
    pub savings: Time,
    /// The start time for the transition
    pub start: PosixDateTime,
    /// The end time for the transition
    pub end: PosixDateTime,
}

#[non_exhaustive]
#[derive(Debug, PartialEq, Clone)]
pub struct PosixAbbreviation {
    /// Flag whether formatted abbreviation is numeric
    pub is_numeric: bool,
    /// The formatted abbreviation
    pub formatted: String,
}
#[derive(Debug, PartialEq, Clone, Copy)]
pub struct MonthWeekDay(pub Month, pub u8, pub WeekDay);

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum PosixDate {
    JulianNoLeap(u16),
    JulianLeap(u16),
    MonthWeekDay(MonthWeekDay),
}

impl PosixDate {
    pub(crate) fn from_rule(rule: &Rule) -> Self {
        match rule.on_date {
            DayOfMonth::Day(day) if rule.in_month == Month::Jan || rule.in_month == Month::Feb => {
                PosixDate::JulianNoLeap(month_to_day(rule.in_month as u8, 1) as u16 + day as u16)
            }
            DayOfMonth::Day(day) => {
                PosixDate::JulianLeap(month_to_day(rule.in_month as u8, 1) as u16 + day as u16)
            }
            DayOfMonth::Last(wd) => PosixDate::MonthWeekDay(MonthWeekDay(rule.in_month, 5, wd)),
            DayOfMonth::WeekDayGEThanMonthDay(week_day, day_of_month) => {
                let week = 1 + (day_of_month - 1) / 7;
                PosixDate::MonthWeekDay(MonthWeekDay(rule.in_month, week, week_day))
            }
            DayOfMonth::WeekDayLEThanMonthDay(week_day, day_of_month) => {
                let week = day_of_month / 7;
                PosixDate::MonthWeekDay(MonthWeekDay(rule.in_month, week, week_day))
            }
        }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct PosixDateTime {
    pub date: PosixDate,
    pub time: Time,
}

impl PosixDateTime {
    pub(crate) fn from_rule_and_transition_info(rule: &Rule, offset: Time, savings: Time) -> Self {
        let date = PosixDate::from_rule(rule);
        let time = match rule.at {
            QualifiedTime::Local(time) => time,
            QualifiedTime::Standard(standard_time) => standard_time.add(rule.save),
            QualifiedTime::Universal(universal_time) => universal_time.add(offset).add(savings),
        };
        Self { date, time }
    }
}

// ==== Helper functions ====

fn is_numeric(str: &str) -> bool {
    str.parse::<i16>().is_ok()
}

fn write_abbr(posix_abbr: &PosixAbbreviation, output: &mut String) -> core::fmt::Result {
    if posix_abbr.is_numeric {
        write!(output, "<")?;
        write!(output, "{}", posix_abbr.formatted)?;
        write!(output, ">")?;
        return Ok(());
    }
    write!(output, "{}", posix_abbr.formatted)
}

fn write_inverted_time(time: &Time, output: &mut String) -> core::fmt::Result {
    // Yep, it's inverted
    if time.sign == Sign::Positive && time.hour != 0 {
        write!(output, "-")?;
    }
    write_time(time, output)
}

fn write_time(time: &Time, output: &mut String) -> core::fmt::Result {
    write!(output, "{}", time.hour)?;
    if time.minute == 0 && time.second == 0 {
        return Ok(());
    }
    write!(output, ":{}", time.minute)?;
    if time.second > 0 {
        write!(output, ":{}", time.second)?;
    }
    Ok(())
}

fn write_date_time(datetime: &PosixDateTime, output: &mut String) -> core::fmt::Result {
    write!(output, ",")?;
    match datetime.date {
        PosixDate::JulianLeap(d) => write!(output, "{d}")?,
        PosixDate::JulianNoLeap(d) => write!(output, "J{d}")?,
        PosixDate::MonthWeekDay(MonthWeekDay(month, week, day)) => {
            write!(output, "M{}.{week}.{}", month as u8, day as u8)?
        }
    }
    if datetime.time != Time::two_hour() {
        write!(output, "/")?;
        write_time(&datetime.time, output)?;
    }
    Ok(())
}
