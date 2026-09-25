// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::preferences::extensions::unicode::enum_keyword;

enum_keyword!(
    /// A Unicode Hour Cycle Identifier defines the preferred time cycle. Specifying "hc" in a locale identifier overrides the default value specified by supplemental time data for the region.
    ///
    /// The valid values are listed in [LDML](https://unicode.org/reports/tr35/#UnicodeHourCycleIdentifier).
    HourCycle {
        /// The 12-hour clock used in much of the world. Hours are numbered 1–12.
        /// 
        /// Corresponds to 'h' in UTS #35 pattern strings.
        ("h12" => H12),
        /// The 24-hour clock. Hour are numbered 0–23.
        /// 
        /// Corresponds to 'H' in UTS #35 pattern strings.
        ("h23" => H23),
        /// Variant of the 12-hour clock, sometimes used in Japan. Hours are numbered 0–11.
        /// 
        /// Corresponds to 'K' in UTS #35 pattern strings.
        ("h11" => H11),
        /// The 12-hour clock automatically selected based on the locale. Hours are numbered
        /// 1-12, 0-11, or another 12-hour clock based on locale data.
        /// 
        /// Corresponds to 'j' in UTS #35 skeleton strings,
        /// except it can resolve to only `h` or `K`.
        ("c12" => Clock12),
        /// The 24-hour clock automatically selected based on the locale. Hours are numbered
        /// 0-23 or another 24-hour clock based on locale data.
        /// 
        /// Corresponds to 'j' in UTS #35 skeleton strings,
        /// except it can resolve to only `H`.
        ("c24" => Clock24),
}, "hc");
