//! A library for parsing and compiling zoneinfo files into
//! time zone transition data that can be used to build
//! TZif files or any other desired time zone format.
//!
//! `zoneinfo_rs` offers default parsing and compiling
//! of zoneinfo files into time zone transition data.
//!
//! Why `zoneinfo_rs`?
//!
//! In general, this library seeks to maximally expose as much
//! data from the zoneinfo files as possible while also supporting
//! extra time zone database features like the zone.tab, PACKRATLIST,
//! and POSIX time zone strings.
//!

// TODO list:
//
//  - Support PACKRATLIST
//  - Support zone.tab
//  - Support leap second
//  - Support vanguard and rear guard parsing (potential backlog)
//  - Provide easy defaults for SLIM and FAT compiling.
//  - Support v1 TZif with conversion to i32.
//

// Implementation note: this library is NOT designed to be the most
// optimal speed. Instead invariance and clarity is preferred where
// need be.
//
// We can get away with any performance penalty primarily because
// this library is designed to aid with build time libraries, on
// a limited dataset, NOT at runtime on extremely large datasets.

#![no_std]

extern crate alloc;

use alloc::string::String;
use parser::ZoneInfoParseError;

use hashbrown::HashMap;

#[cfg(feature = "std")]
extern crate std;

#[cfg(feature = "std")]
use std::{io, path::Path};

pub(crate) mod utils;

pub mod compiler;
pub mod parser;
pub mod posix;
pub mod rule;
pub mod types;
pub mod tzif;
pub mod zone;

#[doc(inline)]
pub use compiler::ZoneInfoCompiler;

#[doc(inline)]
pub use parser::ZoneInfoParser;

use rule::Rules;
use zone::ZoneRecord;

/// Well-known zone info file
pub const ZONEINFO_FILES: [&str; 9] = [
    "africa",
    "antarctica",
    "asia",
    "australasia",
    "backward",
    "etcetera",
    "europe",
    "northamerica",
    "southamerica",
];

/// The general error type for `ZoneInfo` operations
#[derive(Debug)]
pub enum ZoneInfoError {
    Parse(ZoneInfoParseError),
    #[cfg(feature = "std")]
    Io(io::Error),
}

#[cfg(feature = "std")]
impl From<io::Error> for ZoneInfoError {
    fn from(value: io::Error) -> Self {
        Self::Io(value)
    }
}

/// `ZoneInfoData` represents raw unprocessed zone info data
/// as parsed from a zone info file.
///
/// See [`ZoneInfoCompiler`] if transitions are required.
#[non_exhaustive]
#[derive(Debug, Clone, Default)]
pub struct ZoneInfoData {
    /// Data parsed from zone info Rule lines keyed by Rule name
    pub rules: HashMap<String, Rules>,
    /// Data parsed from zone info Zone records.
    pub zones: HashMap<String, ZoneRecord>,
    /// Data parsed from Link lines
    pub links: HashMap<String, String>,
    /// Data parsed from `#PACKRATLIST` lines
    pub pack_rat: HashMap<String, String>,
}

// ==== ZoneInfoData parsing methods ====

impl ZoneInfoData {
    /// Parse data from a path to a directory of zoneinfo files, using well known
    /// zoneinfo file names.
    ///
    /// This is usually pointed to a "tzdata" directory.
    #[cfg(feature = "std")]
    pub fn from_zoneinfo_directory<P: AsRef<Path>>(dir: P) -> Result<Self, ZoneInfoError> {
        let mut zoneinfo = Self::default();
        for filename in ZONEINFO_FILES {
            let file_path = dir.as_ref().join(filename);
            let parsed = Self::from_filepath(file_path)?;
            zoneinfo.extend(parsed);
        }
        Ok(zoneinfo)
    }

    /// Parse data from a filepath to a zoneinfo file.
    #[cfg(feature = "std")]
    pub fn from_filepath<P: AsRef<Path> + core::fmt::Debug>(
        path: P,
    ) -> Result<Self, ZoneInfoError> {
        Self::from_zoneinfo_file(&std::fs::read_to_string(path)?)
    }

    /// Parses data from a zoneinfo file as a string slice.
    pub fn from_zoneinfo_file(src: &str) -> Result<Self, ZoneInfoError> {
        ZoneInfoParser::from_zoneinfo_str(src)
            .parse()
            .map_err(ZoneInfoError::Parse)
    }

    /// Extend the current `ZoneInfoCompiler` data from another `ZoneInfoCompiler`.
    pub fn extend(&mut self, other: Self) {
        self.rules.extend(other.rules);
        self.zones.extend(other.zones);
        self.links.extend(other.links);
        self.pack_rat.extend(other.pack_rat);
    }
}
