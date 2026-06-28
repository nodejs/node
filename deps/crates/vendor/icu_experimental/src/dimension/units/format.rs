// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::dimension::provider::units::display_names::UnitsDisplayNames;
use fixed_decimal::Decimal;
use icu_decimal::DecimalFormatter;
use icu_plurals::PluralRules;
use writeable::{impl_display_with_writeable, Writeable};

#[derive(Debug)]
pub struct FormattedUnit<'l> {
    pub(crate) value: &'l Decimal,
    // TODO: review using options and essentials.
    // pub(crate) _options: &'l UnitsFormatterOptions,
    // pub(crate) essential: &'l UnitsEssentials<'l>,
    pub(crate) display_name: &'l UnitsDisplayNames<'l>,
    pub(crate) decimal_formatter: &'l DecimalFormatter,
    pub(crate) plural_rules: &'l PluralRules,
}

impl Writeable for FormattedUnit<'_> {
    fn write_to_parts<W>(&self, sink: &mut W) -> Result<(), core::fmt::Error>
    where
        W: writeable::PartsWrite + ?Sized,
    {
        self.display_name
            .patterns
            .get(self.value.into(), self.plural_rules)
            .interpolate((self.decimal_formatter.format(self.value),))
            .write_to_parts(sink)
    }
}

impl_display_with_writeable!(FormattedUnit<'_>);

#[test]
fn test_basic() {
    use icu_locale_core::locale;
    use writeable::assert_writeable_eq;

    use crate::dimension::units::formatter::UnitsFormatter;
    use crate::dimension::units::options::{UnitsFormatterOptions, Width};

    let test_cases = [
        (
            locale!("en-US"),
            "meter",
            "1",
            UnitsFormatterOptions {
                width: Width::Long,
                ..Default::default()
            },
            "1 meter",
        ),
        (
            locale!("en-US"),
            "meter",
            "12345.67",
            UnitsFormatterOptions::default(),
            "12,345.67 m",
        ),
        (
            locale!("en-US"),
            "century",
            "12345.67",
            UnitsFormatterOptions {
                width: Width::Long,
                ..Default::default()
            },
            "12,345.67 centuries",
        ),
        (
            locale!("de-DE"),
            "meter",
            "12345.67",
            UnitsFormatterOptions::default(),
            "12.345,67 m",
        ),
        (
            locale!("ar-EG"),
            "meter",
            "12345.67",
            UnitsFormatterOptions {
                width: Width::Long,
                ..Default::default()
            },
            "١٢٬٣٤٥٫٦٧ متر",
        ),
    ];

    for (locale, unit, value, options, expected) in test_cases {
        let fmt = UnitsFormatter::try_new(locale.into(), unit, options).unwrap();
        let value = value.parse().unwrap();
        assert_writeable_eq!(fmt.format_fixed_decimal(&value), expected);
    }
}
