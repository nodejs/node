//! Configuration for swc
#![cfg_attr(docsrs, feature(doc_cfg))]

#[cfg_attr(docsrs, doc(cfg(feature = "file_pattern")))]
#[cfg(feature = "file_pattern")]
pub mod file_pattern;
#[cfg_attr(docsrs, doc(cfg(feature = "glob")))]
#[cfg(feature = "glob")]
pub mod glob;
pub mod is_module;
pub mod merge;
#[cfg_attr(docsrs, doc(cfg(feature = "regex")))]
#[cfg(feature = "regex")]
pub mod regex;

#[cfg_attr(docsrs, doc(cfg(feature = "regex_js")))]
#[cfg(feature = "regex_js")]
pub mod regex_js;

#[cfg_attr(docsrs, doc(cfg(feature = "sourcemap")))]
#[cfg(feature = "sourcemap")]
pub mod source_map;
pub mod types;
