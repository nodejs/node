// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::{Decimal, Sign};
use icu_decimal::DecimalFormatter;

use crate::alloc::borrow::ToOwned;
use alloc::borrow::Cow;
use writeable::Writeable;

use crate::dimension::provider::percent::PercentEssentials;

use super::options::{Display, PercentFormatterOptions};

struct Append<W1, W2>(W1, W2);
// This allows us to combines two writeables together.
impl<W1: Writeable, W2: Writeable> Writeable for Append<W1, W2> {
    fn write_to<W>(&self, sink: &mut W) -> Result<(), core::fmt::Error>
    where
        W: core::fmt::Write + ?Sized,
    {
        self.0.write_to(sink)?;
        self.1.write_to(sink)
    }
}

#[derive(Debug)]
pub struct FormattedPercent<'l> {
    pub(crate) value: &'l Decimal,
    pub(crate) essential: &'l PercentEssentials<'l>,
    pub(crate) options: &'l PercentFormatterOptions,
    pub(crate) decimal_formatter: &'l DecimalFormatter,
}

impl Writeable for FormattedPercent<'_> {
    fn write_to<W>(&self, sink: &mut W) -> Result<(), core::fmt::Error>
    where
        W: core::fmt::Write + ?Sized,
    {
        // Removing the sign from the value
        let abs_value = match self.value.sign() {
            Sign::Negative => self.value.clone().with_sign(Sign::None),
            _ => self.value.to_owned(),
        };

        let value = self.decimal_formatter.format(&abs_value);

        match self.options.display {
            // In the Standard display, we take the unsigned pattern only when the value is positive.
            Display::Standard => {
                if self.value.sign() == Sign::Negative {
                    self.essential
                        .signed_pattern
                        .interpolate((value, &self.essential.minus_sign))
                        .write_to(sink)?
                } else {
                    self.essential
                        .unsigned_pattern
                        .interpolate([value])
                        .write_to(sink)?
                };
            }
            Display::Approximate => {
                let sign = if self.value.sign() == Sign::Negative {
                    // The approximate sign gets pre-pended
                    Append(
                        &self.essential.approximately_sign,
                        &self.essential.minus_sign,
                    )
                } else {
                    Append(&self.essential.approximately_sign, &Cow::Borrowed(""))
                };

                self.essential
                    .signed_pattern
                    .interpolate((value, sign))
                    .write_to(sink)?;
            }
            Display::ExplicitSign => self
                .essential
                .signed_pattern
                .interpolate((
                    value,
                    if self.value.sign() == Sign::Negative {
                        &self.essential.minus_sign
                    } else {
                        &self.essential.plus_sign
                    },
                ))
                .write_to(sink)?,
        };

        Ok(())
    }
}

writeable::impl_display_with_writeable!(FormattedPercent<'_>);

#[cfg(test)]
mod tests {
    use icu_locale_core::locale;
    use writeable::assert_writeable_eq;

    use crate::dimension::percent::{
        formatter::{PercentFormatter, PercentFormatterPreferences},
        options::{Display, PercentFormatterOptions},
    };

    #[test]
    pub fn test_en_us() {
        let prefs: PercentFormatterPreferences = locale!("en-US").into();
        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let default_fmt = PercentFormatter::try_new(prefs, Default::default()).unwrap();
        let formatted_percent = default_fmt.format(&positive_value);
        assert_writeable_eq!(formatted_percent, "12,345.67%");

        // Negative case
        let neg_value = "-12345.67".parse().unwrap();
        let formatted_percent = default_fmt.format(&neg_value);
        assert_writeable_eq!(formatted_percent, "-12,345.67%");

        // Approximate Case
        let approx_value = "12345.67".parse().unwrap();
        let approx_fmt = PercentFormatter::try_new(
            prefs,
            PercentFormatterOptions {
                display: Display::Approximate,
            },
        )
        .unwrap();
        let formatted_percent = approx_fmt.format(&approx_value);
        assert_writeable_eq!(formatted_percent, "~12,345.67%");

        // ExplicitSign Case
        let explicit_fmt = PercentFormatter::try_new(
            prefs,
            PercentFormatterOptions {
                display: Display::ExplicitSign,
            },
        )
        .unwrap();
        let formatted_percent = explicit_fmt.format(&positive_value);
        assert_writeable_eq!(formatted_percent, "+12,345.67%");
    }

    #[test]
    pub fn test_tr() {
        let prefs: PercentFormatterPreferences = locale!("tr").into();
        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let default_fmt = PercentFormatter::try_new(prefs, Default::default()).unwrap();
        let formatted_percent = default_fmt.format(&positive_value);
        assert_writeable_eq!(formatted_percent, "%12.345,67");

        // Negative case
        let neg_value = "-12345.67".parse().unwrap();
        let formatted_percent = default_fmt.format(&neg_value);
        assert_writeable_eq!(formatted_percent, "-%12.345,67");

        // Approximate Case
        let approx_value = "12345.67".parse().unwrap();
        let approx_fmt = PercentFormatter::try_new(
            prefs,
            PercentFormatterOptions {
                display: Display::Approximate,
            },
        )
        .unwrap();
        let formatted_percent = approx_fmt.format(&approx_value);
        assert_writeable_eq!(formatted_percent, "~%12.345,67");

        // ExplicitSign Case
        let explicit_fmt = PercentFormatter::try_new(
            prefs,
            PercentFormatterOptions {
                display: Display::ExplicitSign,
            },
        )
        .unwrap();
        let formatted_percent = explicit_fmt.format(&positive_value);
        assert_writeable_eq!(formatted_percent, "+%12.345,67");
    }

    #[test]
    pub fn test_blo() {
        let prefs: PercentFormatterPreferences = locale!("blo").into();
        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let default_fmt = PercentFormatter::try_new(prefs, Default::default()).unwrap();
        let formatted_percent = default_fmt.format(&positive_value);
        assert_writeable_eq!(formatted_percent, "%\u{a0}12\u{a0}345,67");

        // Negative case
        let neg_value = "-12345.67".parse().unwrap();
        let formatted_percent = default_fmt.format(&neg_value);
        assert_writeable_eq!(formatted_percent, "%\u{a0}-12\u{a0}345,67");

        // Approximate Case
        let approx_value = "12345.67".parse().unwrap();
        let approx_fmt = PercentFormatter::try_new(
            prefs,
            PercentFormatterOptions {
                display: Display::Approximate,
            },
        )
        .unwrap();
        let formatted_percent = approx_fmt.format(&approx_value);
        assert_writeable_eq!(formatted_percent, "%\u{a0}~12\u{a0}345,67");

        // ExplicitSign Case
        let explicit_fmt = PercentFormatter::try_new(
            prefs,
            PercentFormatterOptions {
                display: Display::ExplicitSign,
            },
        )
        .unwrap();
        let formatted_percent = explicit_fmt.format(&positive_value);
        assert_writeable_eq!(formatted_percent, "%\u{a0}+12\u{a0}345,67");
    }
}
