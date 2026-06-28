// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use tinystr::TinyAsciiStr;

pub mod compact_format;
pub mod compact_formatter;
pub mod format;
pub mod formatter;
pub mod long_compact_format;
pub mod long_compact_formatter;
pub mod long_format;
pub mod long_formatter;
pub mod options;

/// A currency code, such as "USD" or "EUR".
#[derive(Clone, Copy, Debug)]
#[allow(clippy::exhaustive_structs)] // newtype
pub struct CurrencyCode(pub TinyAsciiStr<3>);
