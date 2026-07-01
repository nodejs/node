// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::Cow;
use core::fmt::Write;
use writeable::Part;

use crate::grouper;
use crate::input::Decimal;
use crate::options::*;
use crate::parts;
use crate::provider::*;
use crate::size_test_macro::size_test;
use crate::DecimalFormatterPreferences;
#[cfg(feature = "alloc")]
use alloc::string::String;
use fixed_decimal::{Sign, UnsignedDecimal};
use icu_provider::prelude::*;
use writeable::PartsWrite;
use writeable::Writeable;

size_test!(DecimalFormatter, decimal_formatter_size, 96);

/// A formatter for [`Decimal`], rendering decimal digits in an i18n-friendly way.
///
/// [`DecimalFormatter`] supports:
///
/// 1. Rendering in the local numbering system
/// 2. Locale-sensitive grouping separator positions
/// 3. Locale-sensitive plus and minus signs
///
/// See the crate-level documentation for examples.
#[doc = decimal_formatter_size!()]
#[derive(Debug, Clone)]
pub struct DecimalFormatter {
    options: DecimalFormatterOptions,
    symbols: DataPayload<DecimalSymbolsV1>,
    digits: DataPayload<DecimalDigitsV1>,
}

impl AsRef<DecimalFormatter> for DecimalFormatter {
    fn as_ref(&self) -> &DecimalFormatter {
        self
    }
}

impl DecimalFormatter {
    icu_provider::gen_buffer_data_constructors!(
        (prefs: DecimalFormatterPreferences, options: DecimalFormatterOptions) -> error: DataError,
        /// Creates a new [`DecimalFormatter`] from compiled data and an options bag.
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<
        D: DataProvider<DecimalSymbolsV1> + DataProvider<DecimalDigitsV1> + ?Sized,
    >(
        provider: &D,
        prefs: DecimalFormatterPreferences,
        options: DecimalFormatterOptions,
    ) -> Result<Self, DataError> {
        // In case the user explicitly specified a numbering system, use digits from that numbering system. In case of explicitly specified numbering systems,
        // the resolved one may end up being different due to a lack of data or fallback, e.g. attempting to resolve en-u-nu-thai will likely produce en-u-nu-Latn data.
        //
        // This correctly handles the following cases:
        // - Explicitly specified numbering system that is the same as the resolved numbering system: This code effects no change
        // - Explicitly specified numbering system that is different from the resolved one: This code overrides it, but the symbols are still correctly loaded for the locale
        // - No explicitly specified numbering system: The default numbering system for the locale is used.
        // - Explicitly specified numbering system without data for it: this falls back to the resolved numbering system
        //
        // Assuming the provider has symbols for en-u-nu-latn, th-u-nu-thai (default for th), and th-u-nu-latin, this produces the following behavior:
        //
        // | Input Locale | Symbols | Digits | Return value of `numbering_system()` |
        // |--------------|---------|--------|--------------------------------------|
        // | en           | latn    | latn   | latn                                 |
        // | en-u-nu-thai | latn    | thai   | thai                                 |
        // | th           | thai    | thai   | thai                                 |
        // | th-u-nu-latn | latn    | latn   | latn                                 |
        // | en-u-nu-wxyz | latn    | latn   | latn                                 |
        // | th-u-nu-wxyz | thai    | thai   | thai                                 |

        let locale = DecimalSymbolsV1::make_locale(prefs.locale_preferences);

        // Load symbols for the locale/numsys pair provided
        let symbols = load_with_fallback::<DecimalSymbolsV1>(
            provider,
            // fall back to the locale
            prefs
                .nu_id(&locale)
                .into_iter()
                .chain([DataIdentifierBorrowed::for_locale(&locale)]),
        )?
        .payload;

        let resolved_nu_id = DataIdentifierBorrowed::for_marker_attributes(
            DataMarkerAttributes::from_str_or_panic(symbols.get().numsys()),
        );

        let digits = load_with_fallback(
            provider,
            // fall back to the resolved numbering system
            prefs.nu_id(&locale).into_iter().chain([resolved_nu_id]),
        )?
        .payload;

        Ok(Self {
            options,
            symbols,
            digits,
        })
    }

    /// Formats a [`Decimal`], returning a [`FormattedDecimal`].
    pub fn format<'l>(&'l self, value: &'l Decimal) -> FormattedDecimal<'l> {
        FormattedDecimal(self.format_sign(
            value.sign,
            self.format_unsigned(Cow::Borrowed(&value.absolute)),
        ))
    }

    #[doc(hidden)] // TODO(#3647): should be private
    pub fn format_unsigned<'l>(
        &'l self,
        value: Cow<'l, UnsignedDecimal>,
    ) -> FormattedUnsignedDecimal<'l> {
        FormattedUnsignedDecimal {
            value,
            options: &self.options,
            symbols: self.symbols.get(),
            digits: self.digits.get(),
        }
    }

    #[doc(hidden)] // TODO(#3647): should be private
    pub fn format_sign<'l, T>(&'l self, sign: Sign, value: T) -> FormattedSign<'l, T> {
        FormattedSign {
            sign: match sign {
                Sign::None => None,
                Sign::Negative => Some((
                    parts::MINUS_SIGN,
                    self.symbols.get().strings.minus_sign_prefix(),
                    self.symbols.get().strings.minus_sign_suffix(),
                )),
                Sign::Positive => Some((
                    parts::PLUS_SIGN,
                    self.symbols.get().strings.plus_sign_prefix(),
                    self.symbols.get().strings.plus_sign_suffix(),
                )),
            },
            value,
        }
    }

    /// Formats a [`Decimal`], returning a [`String`].
    ///
    /// ✨ *Enabled with the `alloc` Cargo feature.*
    #[cfg(feature = "alloc")]
    pub fn format_to_string(&self, value: &Decimal) -> String {
        use writeable::Writeable;
        self.format(value).write_to_string().into_owned()
    }
}

/// An intermediate structure returned by [`DecimalFormatter`](crate::DecimalFormatter).
/// Use [`Writeable`][Writeable] to render the formatted decimal to a string or buffer.
#[derive(Debug, PartialEq, Clone)]
pub struct FormattedDecimal<'l>(pub(crate) FormattedSign<'l, FormattedUnsignedDecimal<'l>>);

#[doc(hidden)] // TODO(#3647): should be private
/// Building block for formatted numbers
#[derive(Debug, PartialEq, Clone)]
pub struct FormattedUnsignedDecimal<'l> {
    pub(crate) value: Cow<'l, UnsignedDecimal>,
    pub(crate) options: &'l DecimalFormatterOptions,
    pub(crate) symbols: &'l DecimalSymbols<'l>,
    pub(crate) digits: &'l [char; 10],
}

#[doc(hidden)] // TODO(#3647): should be private
/// Building block for formatted numbers
#[derive(Debug, PartialEq, Clone)]
pub struct FormattedSign<'l, T> {
    pub(crate) value: T,
    pub(crate) sign: Option<(Part, &'l str, &'l str)>,
}

impl<T: Writeable> Writeable for FormattedSign<'_, T> {
    fn write_to_parts<S: PartsWrite + ?Sized>(&self, sink: &mut S) -> core::fmt::Result {
        if let Some((part, prefix, _)) = self.sign {
            sink.with_part(part, |w| w.write_str(prefix))?;
        }
        self.value.write_to_parts(sink)?;
        if let Some((part, _, suffix)) = self.sign {
            sink.with_part(part, |w| w.write_str(suffix))?;
        }
        Ok(())
    }
}

impl Writeable for FormattedUnsignedDecimal<'_> {
    fn write_to_parts<W>(&self, w: &mut W) -> Result<(), core::fmt::Error>
    where
        W: PartsWrite + ?Sized,
    {
        let range = self.value.magnitude_range();
        let upper_magnitude = *range.end();
        let mut range = range.rev().peekable();
        w.with_part(parts::INTEGER, |w| {
            while let Some(m) = range.next_if(|&m| m >= 0) {
                #[expect(clippy::indexing_slicing)] // digit_at in 0..=9
                w.write_char(self.digits[self.value.digit_at(m) as usize])?;
                if grouper::check(
                    upper_magnitude,
                    m,
                    self.options.grouping_strategy.unwrap_or_default(),
                    self.symbols.grouping_sizes,
                ) {
                    w.with_part(parts::GROUP, |w| {
                        w.write_str(self.symbols.grouping_separator())
                    })?;
                }
            }
            Ok(())
        })?;
        if range.peek().is_some() {
            w.with_part(parts::DECIMAL, |w| {
                w.write_str(self.symbols.decimal_separator())
            })?;
            w.with_part(parts::FRACTION, |w| {
                for m in range.by_ref() {
                    #[expect(clippy::indexing_slicing)] // digit_at in 0..=9
                    w.write_char(self.digits[self.value.digit_at(m) as usize])?;
                }
                Ok(())
            })?;
        }
        Ok(())
    }
}

impl Writeable for FormattedDecimal<'_> {
    fn write_to_parts<W>(&self, sink: &mut W) -> Result<(), core::fmt::Error>
    where
        W: PartsWrite + ?Sized,
    {
        self.0.write_to_parts(sink)
    }
}

writeable::impl_display_with_writeable!(FormattedDecimal<'_>, #[cfg(feature = "alloc")]);
writeable::impl_display_with_writeable!(FormattedUnsignedDecimal<'_>, #[cfg(feature = "alloc")]);

/// This trait is implemented for compatibility with [`fmt!`](core::fmt)
/// To create a string, [`Writeable::write_to_string`] is usually more efficient
impl<T: Writeable> core::fmt::Display for FormattedSign<'_, T> {
    #[inline]
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
        Writeable::write_to(&self, f)
    }
}
#[cfg(feature = "alloc")]
impl<T: Writeable> FormattedSign<'_, T> {
    /// Converts the given value to a `String`.
    ///
    /// Under the hood, this uses an efficient [`Writeable`] implementation.
    /// However, in order to avoid allocating a string, it is more efficient
    /// to use [`Writeable`] directly.
    #[allow(clippy::inherent_to_string_shadow_display)]
    pub fn to_string(&self) -> String {
        Writeable::write_to_string(self).into_owned()
    }
}

#[test]
fn test_numbering_resolution_fallback() {
    use icu_locale_core::locale;
    fn test_locale(locale: icu_locale_core::Locale, expected_format: &str) {
        let formatter =
            DecimalFormatter::try_new((&locale).into(), Default::default()).expect("Must load");
        let fd = 1234.into();
        writeable::assert_writeable_eq!(
            formatter.format(&fd),
            expected_format,
            "Correct format for {locale}"
        );
    }

    // Loading en with default latn numsys
    test_locale(locale!("en"), "1,234");
    // Loading en with arab numsys not present in symbols data will mix en symbols with arab digits
    test_locale(locale!("en-u-nu-arab"), "١,٢٣٤");
    // Loading ar-EG with default (arab) numsys
    test_locale(locale!("ar-EG"), "١٬٢٣٤");
    // Loading ar-EG with overridden latn numsys, present in symbols data, uses ar-EG-u-nu-latn symbols data
    test_locale(locale!("ar-EG-u-nu-latn"), "1,234");
    // Loading ar-EG with overridden thai numsys, not present in symbols data, uses ar-EG symbols data + thai digits
    test_locale(locale!("ar-EG-u-nu-thai"), "๑٬๒๓๔");
    // Loading with nonexistant numbering systems falls back to default
    test_locale(locale!("en-u-nu-wxyz"), "1,234");
    test_locale(locale!("ar-EG-u-nu-wxyz"), "١٬٢٣٤");
}

#[test]
pub fn test_es_mx() {
    use icu_locale_core::locale;
    use writeable::assert_writeable_eq;
    let locale = locale!("es-MX").into();
    let fmt = DecimalFormatter::try_new(locale, Default::default()).unwrap();
    let fd = "12345.67".parse().unwrap();
    assert_writeable_eq!(fmt.format(&fd), "12,345.67");
}
