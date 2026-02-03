// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Types for individual calendars
pub(crate) mod buddhist;
pub(crate) mod chinese;
pub(crate) mod chinese_based;
pub(crate) mod coptic;
pub(crate) mod dangi;
pub(crate) mod ethiopian;
pub(crate) mod gregorian;
pub(crate) mod hebrew;
pub(crate) mod hijri;
pub(crate) mod indian;
pub(crate) mod iso;
pub(crate) mod japanese;
pub(crate) mod julian;
pub(crate) mod persian;
pub(crate) mod roc;

pub use buddhist::Buddhist;
pub use chinese::Chinese;
pub use coptic::Coptic;
pub use dangi::Dangi;
pub use ethiopian::{Ethiopian, EthiopianEraStyle};
pub use gregorian::Gregorian;
pub use hebrew::Hebrew;
pub use hijri::{
    HijriSimulated, HijriTabular, HijriTabularEpoch, HijriTabularLeapYears, HijriUmmAlQura,
};
pub use indian::Indian;
pub use iso::Iso;
pub use japanese::{Japanese, JapaneseExtended};
pub use julian::Julian;
pub use persian::Persian;
pub use roc::Roc;

pub use crate::any_calendar::{AnyCalendar, AnyCalendarKind};

/// Internal scaffolding types
pub mod scaffold {
    /// Trait marking other traits that are considered unstable and should not generally be
    /// implemented outside of the calendar crate.
    ///
    /// <div class="stab unstable">
    /// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
    /// </div>
    pub trait UnstableSealed {}
}
