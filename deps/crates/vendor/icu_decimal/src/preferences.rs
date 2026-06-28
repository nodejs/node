// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Locale preferences used by this crate

use icu_locale_core::preferences::define_preferences;
#[cfg(feature = "unstable")]
use icu_locale_core::preferences::prefs_convert;

define_preferences!(
    /// The preferences for fixed decimal formatting.
    [Copy]
    DecimalFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        ///
        /// To get the resolved numbering system, you can inspect the data provider.
        /// See the [`provider`] module for an example.
        numbering_system: NumberingSystem
    }
);

/// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
#[doc = "\n"] // prevent autoformatting
pub use icu_locale_core::preferences::extensions::unicode::keywords::NumberingSystem;

#[cfg(feature = "unstable")]
define_preferences!(
    /// The preferences for compact decimal formatting.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #7161](https://github.com/unicode-org/icu4x/issues/7161).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    [Copy]
    CompactDecimalFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        numbering_system: NumberingSystem
    }
);

#[cfg(feature = "unstable")]
prefs_convert!(
    CompactDecimalFormatterPreferences,
    DecimalFormatterPreferences,
    { numbering_system }
);
#[cfg(feature = "unstable")]
prefs_convert!(
    CompactDecimalFormatterPreferences,
    icu_plurals::PluralRulesPreferences
);
