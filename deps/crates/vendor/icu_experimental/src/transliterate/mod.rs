// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Transliteration
//!
//! See [`Transliterator`].

pub mod provider;

mod compile;
mod transliterator;

#[cfg(feature = "compiled_data")]
pub use transliterator::TransliteratorBuilder;
pub use transliterator::{CustomTransliterator, Transliterator};

pub use compile::RuleCollection;
pub use compile::RuleCollectionProvider;
