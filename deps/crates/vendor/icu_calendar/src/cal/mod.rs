// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Types for individual calendars
pub(crate) mod buddhist;
pub(crate) mod coptic;
#[path = "east_asian_traditional.rs"]
pub(crate) mod east_asian_traditional_internal;
pub(crate) mod ethiopian;
pub(crate) mod gregorian;
pub(crate) mod hebrew;
#[path = "hijri.rs"]
pub(crate) mod hijri_internal;
pub(crate) mod indian;
pub(crate) mod iso;
pub(crate) mod japanese;
pub(crate) mod julian;
pub(crate) mod persian;
pub(crate) mod roc;

pub(crate) mod abstract_gregorian;

pub use buddhist::Buddhist;
/// Customizations for the [`EastAsianTraditional`](east_asian_traditional::EastAsianTraditional) calendar.
pub mod east_asian_traditional {
    pub use super::east_asian_traditional_internal::{China, EastAsianTraditional, Korea};

    // TODO(#6962) Stabilize
    #[cfg(feature = "unstable")]
    pub use super::east_asian_traditional_internal::{EastAsianTraditionalYearData, Rules};
}
pub use coptic::Coptic;
pub use east_asian_traditional_internal::{ChineseTraditional, KoreanTraditional};
pub use ethiopian::{Ethiopian, EthiopianEraStyle};
pub use gregorian::Gregorian;
pub use hebrew::Hebrew;
pub use hijri_internal::Hijri;
/// Customizations for the [`Hijri`] calendar.
pub mod hijri {
    pub use super::hijri_internal::{
        AstronomicalSimulation, TabularAlgorithm, TabularAlgorithmEpoch, TabularAlgorithmLeapYears,
        UmmAlQura,
    };

    // TODO(#6962) Stabilize
    #[cfg(feature = "unstable")]
    pub use super::hijri_internal::{HijriYearData, Rules};

    #[doc(hidden)]
    /// These are unstable traits but we expose them on stable to
    /// icu_datetime.
    pub mod unstable_internal {
        pub use super::super::hijri_internal::Rules;
    }
}

pub use indian::Indian;
pub use iso::Iso;
pub use japanese::{Japanese, JapaneseExtended};
pub use julian::Julian;
pub use persian::Persian;
pub use roc::Roc;

/// Deprecated
#[deprecated]
pub use hijri::{
    TabularAlgorithmEpoch as HijriTabularEpoch, TabularAlgorithmLeapYears as HijriTabularLeapYears,
};
/// Deprecated
#[deprecated]
pub type HijriSimulated = Hijri<hijri::AstronomicalSimulation>;
/// Deprecated
#[deprecated]
pub type HijriUmmAlQura = Hijri<hijri::UmmAlQura>;
/// Deprecated
#[deprecated]
pub type HijriTabular = Hijri<hijri::TabularAlgorithm>;
/// Use [`KoreanTraditional`]
#[deprecated(since = "2.1.0", note = "use `KoreanTraditional`")]
pub type Dangi = KoreanTraditional;
/// Use [`ChineseTraditional`]
#[deprecated(since = "2.1.0", note = "use `ChineseTraditional`")]
pub type Chinese = ChineseTraditional;

pub use crate::any_calendar::{AnyCalendar, AnyCalendarDifferenceError, AnyCalendarKind};

/// Internal scaffolding types
#[cfg_attr(not(feature = "unstable"), doc(hidden))]
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
