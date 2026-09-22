// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::extensions::unicode::{Key, key};
use crate::preferences::extensions::unicode::errors::PreferencesParseError;
use crate::preferences::extensions::unicode::struct_keyword;
use crate::{extensions::unicode::Value, subtags::Subtag};
use tinystr::TinyAsciiStr;

impl_tinystr_subtag!(
    /// A Unicode Currency Identifier defines a type of currency.
    ///
    /// The valid values are listed in [LDML](https://unicode.org/reports/tr35/#UnicodeCurrencyIdentifier).
    CurrencyType,
    preferences::extensions::unicode::keywords,
    currency,
    preferences_extensions_unicode_keywords_currency,
    3..=3,
    s,
    s.is_ascii_alphabetic(),
    s.to_ascii_lowercase(),
    s.is_ascii_alphabetic() && s.is_ascii_lowercase(),
    InvalidExtension,
    ["usd"],
    ["dollar"],
);

impl CurrencyType {
    /// Returns the ISO 4217 3-letter upper case currency code as a [`TinyAsciiStr<3>`].
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_locale_core::preferences::extensions::unicode::keywords::CurrencyType;
    /// use tinystr::tinystr;
    ///
    /// let currency = CurrencyType::try_from_str("usd").unwrap();
    /// assert_eq!(currency.iso_code(), tinystr!(3, "USD"));
    /// ```
    #[inline]
    pub const fn iso_code(self) -> TinyAsciiStr<3> {
        self.0.to_ascii_uppercase()
    }
}

impl TryFrom<Value> for CurrencyType {
    type Error = PreferencesParseError;
    fn try_from(input: Value) -> Result<Self, Self::Error> {
        Self::try_from(&input)
    }
}

impl TryFrom<&Value> for CurrencyType {
    type Error = PreferencesParseError;
    fn try_from(input: &Value) -> Result<Self, Self::Error> {
        if let Some(subtag) = input.as_single_subtag() {
            let ts = subtag.as_tinystr();
            if ts.len() == 3 && ts.is_ascii_alphabetic() {
                return Ok(Self(ts.resize()));
            }
        }
        Err(PreferencesParseError::InvalidKeywordValue)
    }
}

impl From<CurrencyType> for Value {
    fn from(input: CurrencyType) -> Value {
        (&input).into()
    }
}
impl From<&CurrencyType> for Value {
    fn from(input: &CurrencyType) -> Value {
        Value::from_subtag(Some(Subtag::from_tinystr_unvalidated(input.0.resize())))
    }
}
impl crate::preferences::PreferenceKey for CurrencyType {
    fn unicode_extension_key() -> Option<Key> {
        Some(Self::UNICODE_EXTENSION_KEY)
    }
    fn try_from_key_value(key: &Key, value: &Value) -> Result<Option<Self>, PreferencesParseError> {
        if Self::UNICODE_EXTENSION_KEY == *key {
            let result = Self::try_from(value.clone())?;
            Ok(Some(result))
        } else {
            Ok(None)
        }
    }
    fn unicode_extension_value(&self) -> Option<Value> {
        Some(self.into())
    }
}
impl CurrencyType {
    pub(crate) const UNICODE_EXTENSION_KEY: Key = key!("cu");
}
impl core::ops::Deref for CurrencyType {
    type Target = TinyAsciiStr<3>;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tinystr::tinystr;

    #[test]
    fn test_valid_currency_types() {
        let valid = [
            ("USD", "usd", "USD"),
            ("uSd", "usd", "USD"),
            ("usd", "usd", "USD"),
            ("EUR", "eur", "EUR"),
            ("JPY", "jpy", "JPY"),
        ];
        for (input, expected_subtag, expected_iso) in valid {
            let parsed = CurrencyType::try_from_str(input).unwrap();
            let expected_ts_iso = TinyAsciiStr::<3>::try_from_str(expected_iso).unwrap();
            assert_eq!(parsed.as_str(), expected_subtag);
            assert_eq!(parsed.iso_code(), expected_ts_iso);
            assert_eq!(parsed, input.parse::<CurrencyType>().unwrap());
        }
    }

    #[test]
    fn test_invalid_currency_types() {
        let invalid = [
            "", "U", "US", "USDDD", "US1", "123", "U$D", " US", "US ", "ÉUR",
        ];
        for input in invalid {
            assert!(CurrencyType::try_from_str(input).is_err());
            assert!(input.parse::<CurrencyType>().is_err());
        }
    }
}
