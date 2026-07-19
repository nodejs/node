// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::UtcOffset;
use crate::{Offset, PossibleOffset, Zone};
use chrono::{
    Datelike, FixedOffset, MappedLocalTime, NaiveDate, NaiveDateTime, TimeZone, Timelike,
};

#[derive(Clone, Copy, Debug)]
pub struct ChronoOffset<'a>(Offset, Zone<'a>);

impl core::ops::Deref for ChronoOffset<'_> {
    type Target = UtcOffset;

    fn deref(&self) -> &Self::Target {
        &self.0.offset
    }
}

impl ChronoOffset<'_> {
    pub fn rule_applies(&self) -> bool {
        self.0.rule_applies
    }
}

impl<'a> chrono::Offset for ChronoOffset<'a> {
    fn fix(&self) -> FixedOffset {
        FixedOffset::east_opt(self.0.offset.0).unwrap()
    }
}

impl<'a> TimeZone for Zone<'a> {
    type Offset = ChronoOffset<'a>;

    fn from_offset(offset: &Self::Offset) -> Self {
        offset.1
    }

    fn offset_from_local_date(&self, local: &NaiveDate) -> MappedLocalTime<Self::Offset> {
        match self.for_date_time(
            local.year(),
            local.month() as u8,
            local.day() as u8,
            0,
            0,
            0,
        ) {
            PossibleOffset::None { .. } => chrono::MappedLocalTime::None,
            PossibleOffset::Single(o) => chrono::MappedLocalTime::Single(ChronoOffset(o, *self)),
            PossibleOffset::Ambiguous { before, after, .. } => {
                MappedLocalTime::Ambiguous(ChronoOffset(before, *self), ChronoOffset(after, *self))
            }
        }
    }

    fn offset_from_local_datetime(&self, local: &NaiveDateTime) -> MappedLocalTime<Self::Offset> {
        match self.for_date_time(
            local.year(),
            local.month() as u8,
            local.day() as u8,
            local.hour() as u8,
            local.minute() as u8,
            local.second() as u8,
        ) {
            PossibleOffset::None { .. } => chrono::MappedLocalTime::None,
            PossibleOffset::Single(o) => chrono::MappedLocalTime::Single(ChronoOffset(o, *self)),
            PossibleOffset::Ambiguous { before, after, .. } => {
                MappedLocalTime::Ambiguous(ChronoOffset(before, *self), ChronoOffset(after, *self))
            }
        }
    }

    fn offset_from_utc_date(&self, utc: &NaiveDate) -> Self::Offset {
        ChronoOffset(
            self.for_timestamp(utc.and_time(chrono::NaiveTime::MIN).and_utc().timestamp()),
            *self,
        )
    }

    fn offset_from_utc_datetime(&self, utc: &NaiveDateTime) -> Self::Offset {
        ChronoOffset(self.for_timestamp(utc.and_utc().timestamp()), *self)
    }
}
