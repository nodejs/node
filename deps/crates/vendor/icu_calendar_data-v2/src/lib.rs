// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data for the `icu_calendar` crate
//!
//! This data was generated with CLDR version 49.0.0, Unicode version 18.0.0, and
//! LSTM segmenter version v0.1.0.

#![no_std]
// The source is not readable and is massive as HTML.
#![doc(html_no_source)]

#[cfg(icu4x_custom_data)]
include!(concat!(core::env!("ICU4X_DATA_DIR"), "/mod.rs"));
#[cfg(not(icu4x_custom_data))]
include!("../data/mod.rs");

#[macro_export]
macro_rules! cldr_tag {
    () => {
        "49.0.0"
    };
}

#[macro_export]
macro_rules! unicode_tag {
    () => {
        "18.0.0"
    };
}

#[macro_export]
macro_rules! segmenter_lstm_tag {
    () => {
        "v0.1.0"
    };
}
