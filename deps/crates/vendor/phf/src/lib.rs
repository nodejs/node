//! Rust-PHF is a library to generate efficient lookup tables at compile time using
//! [perfect hash functions](http://en.wikipedia.org/wiki/Perfect_hash_function).
//!
//! It currently uses the
//! [CHD algorithm](http://cmph.sourceforge.net/papers/esa09.pdf) and can generate
//! a 100,000 entry map in roughly .4 seconds.
//!
//! MSRV (minimum supported rust version) is Rust 1.61.
//!
//! ## Usage
//!
//! PHF data structures can be constructed via either the procedural
//! macros in the `phf_macros` crate or code generation supported by the
//! `phf_codegen` crate. If you prefer macros, you can easily use them by
//! enabling the `macros` feature of the `phf` crate, like:
//!
//!```toml
//! [dependencies]
//! phf = { version = "0.11", features = ["macros"] }
//! ```
//!
//! To compile the `phf` crate with a dependency on
//! libcore instead of libstd, enabling use in environments where libstd
//! will not work, set `default-features = false` for the dependency:
//!
//! ```toml
//! [dependencies]
//! # to use `phf` in `no_std` environments
//! phf = { version = "0.11", default-features = false }
//! ```
//!
//! ## Example (with the `macros` feature enabled)
//!
//! ```rust
//! use phf::phf_map;
//!
//! #[derive(Clone)]
//! pub enum Keyword {
//!     Loop,
//!     Continue,
//!     Break,
//!     Fn,
//!     Extern,
//! }
//!
//! static KEYWORDS: phf::Map<&'static str, Keyword> = phf_map! {
//!     "loop" => Keyword::Loop,
//!     "continue" => Keyword::Continue,
//!     "break" => Keyword::Break,
//!     "fn" => Keyword::Fn,
//!     "extern" => Keyword::Extern,
//! };
//!
//! pub fn parse_keyword(keyword: &str) -> Option<Keyword> {
//!     KEYWORDS.get(keyword).cloned()
//! }
//! ```
//!
//! Alternatively, you can use the [`phf_codegen`] crate to generate PHF datatypes
//! in a build script.
//!
//! [`phf_codegen`]: https://docs.rs/phf_codegen
//!
//! ## Note
//!
//! Currently, the macro syntax has some limitations and may not
//! work as you want. See [#183] or [#196] for example.
//!
//! [#183]: https://github.com/rust-phf/rust-phf/issues/183
//! [#196]: https://github.com/rust-phf/rust-phf/issues/196

#![doc(html_root_url = "https://docs.rs/phf/0.11")]
#![warn(missing_docs)]
#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(feature = "std")]
extern crate std as core;

#[cfg(feature = "macros")]
/// Macro to create a `static` (compile-time) [`Map`].
///
/// Requires the `macros` feature.
///
/// Supported key expressions are:
/// - literals: bools, (byte) strings, bytes, chars, and integers (these must have a type suffix)
/// - arrays of `u8` integers
/// - dereferenced byte string literals
/// - `UniCase::unicode(string)` or `UniCase::ascii(string)` if the `unicase` feature is enabled
///
/// # Example
///
/// ```
/// use phf::{phf_map, Map};
///
/// static MY_MAP: Map<&'static str, u32> = phf_map! {
///     "hello" => 1,
///     "world" => 2,
/// };
///
/// fn main () {
///     assert_eq!(MY_MAP["hello"], 1);
/// }
/// ```
pub use phf_macros::phf_map;

#[cfg(feature = "macros")]
/// Macro to create a `static` (compile-time) [`OrderedMap`].
///
/// Requires the `macros` feature. Same usage as [`phf_map`].
pub use phf_macros::phf_ordered_map;

#[cfg(feature = "macros")]
/// Macro to create a `static` (compile-time) [`Set`].
///
/// Requires the `macros` feature.
///
/// # Example
///
/// ```
/// use phf::{phf_set, Set};
///
/// static MY_SET: Set<&'static str> = phf_set! {
///     "hello world",
///     "hola mundo",
/// };
///
/// fn main () {
///     assert!(MY_SET.contains("hello world"));
/// }
/// ```
pub use phf_macros::phf_set;

#[cfg(feature = "macros")]
/// Macro to create a `static` (compile-time) [`OrderedSet`].
///
/// Requires the `macros` feature. Same usage as [`phf_set`].
pub use phf_macros::phf_ordered_set;

#[doc(inline)]
pub use self::map::Map;
#[doc(inline)]
pub use self::ordered_map::OrderedMap;
#[doc(inline)]
pub use self::ordered_set::OrderedSet;
#[doc(inline)]
pub use self::set::Set;
pub use phf_shared::PhfHash;

pub mod map;
pub mod ordered_map;
pub mod ordered_set;
pub mod set;
