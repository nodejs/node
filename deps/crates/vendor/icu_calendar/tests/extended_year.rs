// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::types::MonthCode;
use icu_calendar::AnyCalendar;
use icu_calendar::AnyCalendarKind;
use icu_calendar::Date;

#[cfg(test)]
use std::rc::Rc;

/// Reference: <https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-calendardatearithmeticyear>
static EXTENDED_EPOCHS: &[(AnyCalendarKind, i32)] = &[
    (AnyCalendarKind::Buddhist, -543),
    (AnyCalendarKind::Chinese, 0),
    (AnyCalendarKind::Coptic, 283),
    (AnyCalendarKind::Dangi, 0),
    (AnyCalendarKind::Ethiopian, 7),
    (AnyCalendarKind::EthiopianAmeteAlem, -5493),
    (AnyCalendarKind::Gregorian, 0),
    (AnyCalendarKind::Hebrew, -3761),
    (AnyCalendarKind::Indian, 78),
    (AnyCalendarKind::HijriTabularTypeIIFriday, 621),
    (AnyCalendarKind::HijriSimulatedMecca, 621),
    (AnyCalendarKind::HijriTabularTypeIIThursday, 621),
    (AnyCalendarKind::HijriUmmAlQura, 621),
    (AnyCalendarKind::Iso, 0),
    (AnyCalendarKind::Japanese, 0),
    (AnyCalendarKind::JapaneseExtended, 0),
    (AnyCalendarKind::Persian, 621),
    (AnyCalendarKind::Roc, 1911),
];

#[test]
fn test_extended_year() {
    let iso = icu_calendar::cal::Iso;
    let m_01 = MonthCode::new_normal(1).unwrap();
    for (kind, extended_epoch) in EXTENDED_EPOCHS.iter() {
        let calendar = Rc::new(AnyCalendar::new(*kind));

        // Create the first date in the epoch year (extended_year = 0)
        let date_in_epoch_year =
            Date::try_new_from_codes(None, 0, m_01, 1, calendar.clone()).unwrap();
        let iso_date_in_epoch_year = date_in_epoch_year.to_calendar(iso);
        assert_eq!(
            iso_date_in_epoch_year.extended_year(),
            *extended_epoch,
            "Extended year for {date_in_epoch_year:?} should be {extended_epoch}"
        );

        // Test that for all calendars except Japanese, the *current* era is
        // the same as that used for the extended year
        // This date can be anything as long as it is vaguely modern.
        //
        // Note that this property is not *required* by the specification, new
        // calendars may have epochs that do not follow this property. This
        // property is strongly suggested by the specification as a rule of thumb
        // to follow where possible.
        let iso_date_in_2025 = Date::try_new_iso(2025, 1, 1).unwrap();
        let date_in_2025 = iso_date_in_2025.to_calendar(calendar.clone());

        // The extended year should align with the year in the modern era or related ISO.
        // There is a special case for Japanese since it has a modern era but uses ISO for the extended year.
        if matches!(
            kind,
            AnyCalendarKind::Japanese | AnyCalendarKind::JapaneseExtended
        ) {
            assert_eq!(
                date_in_2025.extended_year(),
                2025,
                "Extended year for {date_in_2025:?} should be 2025"
            );
        } else {
            // Note: This code only works because 2025 is in the modern era for all calendars.
            // These two function calls are not equivalent in general.
            let expected = date_in_2025.year().era_year_or_related_iso();
            assert_eq!(
                date_in_2025.extended_year(),
                expected,
                "Extended year for {date_in_2025:?} should be {expected}"
            );
        }
    }
}
