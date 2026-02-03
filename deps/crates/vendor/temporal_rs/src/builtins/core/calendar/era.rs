//! Calendar Eras constants

// The general source for this implementation as of 2024-08-28 is the intl-era-monthcode proposal.
//
// As this source is currently a proposal, its content are subject to change, so full era support
// should be viewed as experimental.
//
// Source: https://tc39.es/proposal-intl-era-monthcode/

// TODO (0.1.0): Feature flag certain eras as experimental

use core::ops::RangeInclusive;

#[cfg(test)]
use icu_calendar::AnyCalendarKind;
use tinystr::{tinystr, TinyAsciiStr};

/// Relevant Era info.
pub(crate) struct EraInfo {
    pub(crate) name: TinyAsciiStr<16>,
    pub(crate) range: RangeInclusive<i32>,
    pub(crate) arithmetic_year: ArithmeticYear,
}

/// The way to map an era to an extended year
pub(crate) enum ArithmeticYear {
    // This era is the default era, 1 ERA = 1 ArithmeticYear
    DefaultEra,
    // This era is a non-default era like reiwa, 1 ERA = offset ArithmeticYear
    Offset(i32),
    // This era is an inverse era like BC, 0 ERA = -1 ArithmeticYear
    Inverse,
}

impl EraInfo {
    pub(crate) fn arithmetic_year_for(&self, era_year: i32) -> i32 {
        match self.arithmetic_year {
            ArithmeticYear::DefaultEra => era_year,
            ArithmeticYear::Offset(offset) => offset + era_year - 1,
            ArithmeticYear::Inverse => 1 - era_year,
        }
    }
}

macro_rules! era_identifier {
    ($name:literal) => {
        tinystr!(19, $name)
    };
}

macro_rules! valid_era {
    ($name:literal, $range:expr, $ext:expr ) => {
        EraInfo {
            name: tinystr!(16, $name),
            range: $range,
            arithmetic_year: $ext,
        }
    };
    ($name:literal, $range:expr ) => {
        valid_era!($name, $range, ArithmeticYear::DefaultEra)
    };
}

pub(crate) const ETHIOPIC_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("am"), era_identifier!("incar")];

pub(crate) const ETHIOPIC_ETHOPICAA_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("aa"), era_identifier!("mundi")];

pub(crate) const ETHIOAA_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("aa"), era_identifier!("mundi")];

pub(crate) const GREGORY_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("ce"), era_identifier!("ad")];

pub(crate) const GREGORY_INVERSE_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("bc"), era_identifier!("bce")];
pub(crate) const JAPANESE_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("ce"), era_identifier!("ad")];

pub(crate) const JAPANESE_INVERSE_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("bc"), era_identifier!("bce")];

// NOTE: The below currently might not align 100% with ICU4X.
// TODO: Update to align with ICU4X depending on any Era updates.
pub(crate) const BUDDHIST_ERA: EraInfo = valid_era!("be", i32::MIN..=i32::MAX);
pub(crate) const COPTIC_ERA: EraInfo = valid_era!("am", 1..=i32::MAX);
pub(crate) const ETHIOPIC_ERA: EraInfo = valid_era!("am", 1..=i32::MAX);
pub(crate) const ETHIOPIC_ETHIOAA_ERA: EraInfo =
    valid_era!("aa", i32::MIN..=5500, ArithmeticYear::Offset(-5499));
pub(crate) const ETHIOAA_ERA: EraInfo = valid_era!("aa", i32::MIN..=i32::MAX);
pub(crate) const GREGORY_ERA: EraInfo = valid_era!("ce", 1..=i32::MAX);
pub(crate) const GREGORY_INVERSE_ERA: EraInfo =
    valid_era!("bce", 1..=i32::MAX, ArithmeticYear::Inverse);
pub(crate) const HEBREW_ERA: EraInfo = valid_era!("am", i32::MIN..=i32::MAX);
pub(crate) const INDIAN_ERA: EraInfo = valid_era!("shaka", i32::MIN..=i32::MAX);
pub(crate) const ISLAMIC_ERA: EraInfo = valid_era!("ah", i32::MIN..=i32::MAX);
pub(crate) const ISLAMIC_INVERSE_ERA: EraInfo =
    valid_era!("bh", i32::MIN..=i32::MAX, ArithmeticYear::Inverse);
pub(crate) const HEISEI_ERA: EraInfo = valid_era!("heisei", 1..=31, ArithmeticYear::Offset(1989));
pub(crate) const JAPANESE_ERA: EraInfo = valid_era!("ce", 1..=1868);
pub(crate) const JAPANESE_INVERSE_ERA: EraInfo =
    valid_era!("bce", 1..=i32::MAX, ArithmeticYear::Inverse);
pub(crate) const MEIJI_ERA: EraInfo = valid_era!("meiji", 1..=45, ArithmeticYear::Offset(1868));
pub(crate) const REIWA_ERA: EraInfo =
    valid_era!("reiwa", 1..=i32::MAX, ArithmeticYear::Offset(2019));
pub(crate) const SHOWA_ERA: EraInfo = valid_era!("showa", 1..=64, ArithmeticYear::Offset(1926));
pub(crate) const TAISHO_ERA: EraInfo = valid_era!("taisho", 1..=45, ArithmeticYear::Offset(1912));
pub(crate) const PERSIAN_ERA: EraInfo = valid_era!("ap", i32::MIN..=i32::MAX);
pub(crate) const ROC_ERA: EraInfo = valid_era!("roc", 1..=i32::MAX);
pub(crate) const ROC_INVERSE_ERA: EraInfo =
    valid_era!("broc", 1..=i32::MAX, ArithmeticYear::Inverse);

#[cfg(test)]
/// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-calendarsupportsera
pub(crate) const ALL_ALLOWED_ERAS: &[(AnyCalendarKind, &[EraInfo])] = &[
    (AnyCalendarKind::Buddhist, &[BUDDHIST_ERA]),
    (AnyCalendarKind::Coptic, &[COPTIC_ERA]),
    (
        AnyCalendarKind::Ethiopian,
        &[ETHIOPIC_ERA, ETHIOPIC_ETHIOAA_ERA],
    ),
    (AnyCalendarKind::EthiopianAmeteAlem, &[ETHIOAA_ERA]),
    (
        AnyCalendarKind::Gregorian,
        &[GREGORY_ERA, GREGORY_INVERSE_ERA],
    ),
    (AnyCalendarKind::Hebrew, &[HEBREW_ERA]),
    (AnyCalendarKind::Indian, &[INDIAN_ERA]),
    (
        AnyCalendarKind::HijriSimulatedMecca,
        &[ISLAMIC_ERA, ISLAMIC_INVERSE_ERA],
    ),
    (
        AnyCalendarKind::HijriTabularTypeIIFriday,
        &[ISLAMIC_ERA, ISLAMIC_INVERSE_ERA],
    ),
    (
        AnyCalendarKind::HijriTabularTypeIIThursday,
        &[ISLAMIC_ERA, ISLAMIC_INVERSE_ERA],
    ),
    (
        AnyCalendarKind::HijriUmmAlQura,
        &[ISLAMIC_ERA, ISLAMIC_INVERSE_ERA],
    ),
    (
        AnyCalendarKind::Japanese,
        &[
            JAPANESE_ERA,
            JAPANESE_INVERSE_ERA,
            MEIJI_ERA,
            REIWA_ERA,
            SHOWA_ERA,
            TAISHO_ERA,
        ],
    ),
    (AnyCalendarKind::Persian, &[PERSIAN_ERA]),
    (AnyCalendarKind::Roc, &[ROC_ERA, ROC_INVERSE_ERA]),
];
