// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// TODO: add more tests for this module to cover more locales & currencies.
#[cfg(test)]
mod tests {
    use icu_locale_core::locale;
    use tinystr::*;
    use writeable::assert_writeable_eq;

    use crate::dimension::currency::{long_formatter::LongCurrencyFormatter, CurrencyCode};

    #[test]
    pub fn test_en_us() {
        let currency_preferences = locale!("en-US").into();
        let currency_code = CurrencyCode(tinystr!(3, "USD"));
        let fmt = LongCurrencyFormatter::try_new(currency_preferences, &currency_code).unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12,345.67 US dollars");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12,345.67 US dollars");
    }

    #[test]
    pub fn test_fr_fr() {
        let currency_preferences = locale!("fr-FR").into();
        let currency_code = CurrencyCode(tinystr!(3, "EUR"));
        let fmt = LongCurrencyFormatter::try_new(currency_preferences, &currency_code).unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12\u{202f}345,67 euros");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12\u{202f}345,67 euros");
    }

    #[test]
    pub fn test_ar_eg() {
        let currency_preferences = locale!("ar-EG").into();
        let currency_code = CurrencyCode(tinystr!(3, "EGP"));
        let fmt = LongCurrencyFormatter::try_new(currency_preferences, &currency_code).unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "١٢٬٣٤٥٫٦٧ جنيه مصري");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "\u{61c}-١٢٬٣٤٥٫٦٧ جنيه مصري");
    }
}
