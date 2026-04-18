use tinystr::TinyAsciiStr;
#[cfg(feature = "datagen")]
use zoneinfo_rs::posix::{MonthWeekDay, PosixDate, PosixDateTime, PosixTimeZone, PosixTransition};

#[zerovec::make_ule(PosixZoneULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
pub struct PosixZone {
    pub abbr: TinyAsciiStr<5>,
    pub offset: i64,
    pub transition: Option<ZeroPosixTransition>,
}

#[cfg(feature = "datagen")]
#[allow(clippy::unwrap_used, reason = "Datagen only")]
impl From<&PosixTimeZone> for PosixZone {
    fn from(value: &PosixTimeZone) -> Self {
        let abbr = TinyAsciiStr::<5>::try_from_str(&value.abbr.formatted).unwrap();
        let offset = value.offset.as_secs();
        let transition = value
            .transition_info
            .as_ref()
            .map(ZeroPosixTransition::from);

        Self {
            abbr,
            offset,
            transition,
        }
    }
}

#[zerovec::make_ule(PosixTransitionULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
pub struct ZeroPosixTransition {
    pub abbr: TinyAsciiStr<5>,
    pub savings: i64,
    pub start: ZeroTransitionDateTime,
    pub end: ZeroTransitionDateTime,
}

#[cfg(feature = "datagen")]
#[allow(clippy::unwrap_used, reason = "Datagen only")]
impl From<&PosixTransition> for ZeroPosixTransition {
    fn from(value: &PosixTransition) -> Self {
        let abbr = TinyAsciiStr::<5>::try_from_str(&value.abbr.formatted).unwrap();
        let savings = value.savings.as_secs();
        let start = ZeroTransitionDateTime::from(&value.start);
        let end = ZeroTransitionDateTime::from(&value.end);
        Self {
            abbr,
            savings,
            start,
            end,
        }
    }
}

#[zerovec::make_ule(TransitionDateTimeULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
pub struct ZeroTransitionDateTime {
    /// The date at which a transition should occur.
    date: ZeroTransitionDate,
    /// The time of day in seconds.
    time: i64,
}

#[cfg(feature = "datagen")]
impl From<&PosixDateTime> for ZeroTransitionDateTime {
    fn from(value: &PosixDateTime) -> Self {
        Self {
            date: value.date.into(),
            time: value.time.as_secs(),
        }
    }
}

#[zerovec::make_ule(DateULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
pub struct ZeroTransitionDate {
    kind: DateKind,
    day: Option<u16>,
    mwd: Option<(u8, u8, u8)>,
}

#[cfg(feature = "datagen")]
impl From<PosixDate> for ZeroTransitionDate {
    fn from(value: PosixDate) -> Self {
        let (kind, day, mwd) = match value {
            PosixDate::JulianLeap(day) => (DateKind::Julian, Some(day), None),
            PosixDate::JulianNoLeap(day) => (DateKind::JulianNoLeap, Some(day), None),
            PosixDate::MonthWeekDay(MonthWeekDay(month, week, day)) => (
                DateKind::MonthWeekDay,
                None,
                Some((month as u8, week, day as u8)),
            ),
        };
        Self { kind, day, mwd }
    }
}
#[zerovec::make_ule(DateKindULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif::posix))]
#[repr(u8)]
pub enum DateKind {
    JulianNoLeap = 0,
    Julian = 1,
    MonthWeekDay = 2,
}
