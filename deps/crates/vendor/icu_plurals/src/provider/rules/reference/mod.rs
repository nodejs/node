// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Reference version of the Plural Rules parser, AST and serializer.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. In particular, the `DataProvider` implementations are only
//! guaranteed to match with this version's `*_unstable` providers. Use with caution.
//! </div>
pub mod ast;
#[cfg(feature = "serde")]
pub(crate) mod lexer;
#[cfg(feature = "serde")]
pub(crate) mod parser;
#[cfg(feature = "datagen")]
pub(crate) mod resolver;
pub(crate) mod serializer;

#[cfg(feature = "datagen")]
pub use lexer::Lexer;
#[cfg(feature = "datagen")]
pub use parser::{parse, parse_condition};
#[cfg(feature = "datagen")]
pub use resolver::test_condition;
pub use serializer::serialize;
