// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(clippy::unwrap_used)] // todo

//! Duration formatting

mod duration;
mod format;
mod formatter;
pub mod provider;

pub mod options;
mod validated_options;

pub use duration::{Duration, DurationSign};
pub use format::FormattedDuration;
pub use formatter::DurationFormatter;
pub use formatter::DurationFormatterPreferences;
pub use validated_options::{DurationFormatterOptionsError, ValidatedDurationFormatterOptions};
