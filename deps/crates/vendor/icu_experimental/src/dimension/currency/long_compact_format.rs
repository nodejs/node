// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[cfg(test)]
mod tests {
    use icu_locale_core::locale;
    use tinystr::*;
    use writeable::assert_writeable_eq;

    use crate::dimension::currency::long_compact_formatter::LongCompactCurrencyFormatter;
    use crate::dimension::currency::CurrencyCode;

    #[test]
    pub fn test_en_us() {
        let currency_formatter_prefs = locale!("en-US").into();

        let currency_code = CurrencyCode(tinystr!(3, "USD"));
        let fmt = LongCompactCurrencyFormatter::try_new(currency_formatter_prefs, &currency_code)
            .unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12 thousand US dollars");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12 thousand US dollars");
    }

    #[test]
    pub fn test_en_us_millions() {
        let currency_formatter_prefs = locale!("en-US").into();

        let currency_code = CurrencyCode(tinystr!(3, "USD"));
        let fmt = LongCompactCurrencyFormatter::try_new(currency_formatter_prefs, &currency_code)
            .unwrap();

        // Positive case
        let positive_value = "12345000.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12 million US dollars");

        // Negative case
        let negative_value = "-12345000.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12 million US dollars");
    }

    #[test]
    pub fn test_fr_fr() {
        let currency_formatter_prefs = locale!("fr-FR").into();

        let currency_code = CurrencyCode(tinystr!(3, "USD"));
        let fmt = LongCompactCurrencyFormatter::try_new(currency_formatter_prefs, &currency_code)
            .unwrap();

        // Positive case
        let positive_value = "12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12 mille dollars des États-Unis");

        // Negative case
        let negative_value = "-12345.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12 mille dollars des États-Unis");
    }

    #[test]
    pub fn test_fr_fr_millions() {
        let currency_formatter_prefs = locale!("fr-FR").into();

        let currency_code = CurrencyCode(tinystr!(3, "USD"));
        let fmt = LongCompactCurrencyFormatter::try_new(currency_formatter_prefs, &currency_code)
            .unwrap();

        // Positive case
        let positive_value = "12345000.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&positive_value);
        assert_writeable_eq!(formatted_currency, "12 millions dollars des États-Unis");

        // Negative case
        let negative_value = "-12345000.67".parse().unwrap();
        let formatted_currency = fmt.format_fixed_decimal(&negative_value);
        assert_writeable_eq!(formatted_currency, "-12 millions dollars des États-Unis");
    }
}
