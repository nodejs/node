//! This library implements basic processing of JavaScript sourcemaps.
//!
//! # Installation
//!
//! The crate is called sourcemap and you can depend on it via cargo:
//!
//! ```toml
//! [dependencies]
//! sourcemap = "*"
//! ```
//!
//! If you want to use the git version:
//!
//! ```toml
//! [dependencies.sourcemap]
//! git = "https://github.com/getsentry/rust-sourcemap.git"
//! ```
//!
//! # Basic Operation
//!
//! This crate can load JavaScript sourcemaps from JSON files.  It uses
//! `serde` for parsing of the JSON data.  Due to the nature of sourcemaps
//! the entirety of the file must be loaded into memory which can be quite
//! memory intensive.
//!
//! Usage:
//!
//! ```rust
//! use swc_sourcemap::SourceMap;
//! let input: &[_] = b"{
//!     \"version\":3,
//!     \"sources\":[\"coolstuff.js\"],
//!     \"names\":[\"x\",\"alert\"],
//!     \"mappings\":\"AAAA,GAAIA,GAAI,EACR,IAAIA,GAAK,EAAG,CACVC,MAAM\"
//! }";
//! let sm = SourceMap::from_reader(input).unwrap();
//! let token = sm.lookup_token(0, 0).unwrap(); // line-number and column
//! println!("token: {}", token);
//! ```
//!
//! # Features
//!
//! Functionality of the crate can be turned on and off by feature flags.  This is the
//! current list of feature flags:
//!
//! * `ram_bundle`: turns on RAM bundle support
//!
pub use crate::builder::SourceMapBuilder;
pub use crate::decoder::{decode, decode_data_url, decode_slice};
pub use crate::detector::{
    is_sourcemap, is_sourcemap_slice, locate_sourcemap_reference, locate_sourcemap_reference_slice,
    SourceMapRef,
};
pub use crate::errors::{Error, Result};
pub use crate::hermes::SourceMapHermes;
pub use crate::sourceview::SourceView;
pub use crate::types::{
    DecodedMap, NameIter, RawToken, RewriteOptions, SourceContentsIter, SourceIter, SourceMap,
    SourceMapIndex, SourceMapSection, SourceMapSectionIter, Token, TokenIter,
};
pub use crate::utils::make_relative_path;

mod builder;
mod decoder;
mod detector;
mod encoder;
mod errors;
mod hermes;
mod js_identifiers;
mod jsontypes;
pub mod lazy;
mod sourceview;
mod types;
mod utils;

#[cfg(feature = "ram_bundle")]
pub mod ram_bundle;
pub mod vlq;
