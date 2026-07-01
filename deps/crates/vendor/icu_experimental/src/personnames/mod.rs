// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(
    clippy::exhaustive_enums,
    clippy::exhaustive_structs,
    missing_docs,
    clippy::unwrap_used
)] // todo

pub use formatter::PersonNamesFormatter;

pub mod api;
pub mod formatter;
pub mod provided_struct;
pub mod provider;

mod specifications;
