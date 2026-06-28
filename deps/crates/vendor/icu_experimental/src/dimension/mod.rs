// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(
    clippy::panic,
    clippy::indexing_slicing,
    clippy::unwrap_used,
    missing_docs
)]

pub mod currency;
pub mod percent;
pub mod provider;
pub mod units;

/// Locale preferences used by this crate
pub mod preferences {
    #[doc(inline)]
    /// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_locale_core::preferences::extensions::unicode::keywords::NumberingSystem;
}
