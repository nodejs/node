//! Checked strings containing Rust identifiers.
//!
//! Raw identifiers are unsupported.
//!
//! # Examples
//!
//! ```rust
//! use strck::{IntoCk, ident::rust::RustIdent};
//!
//! assert!("foo".ck::<RustIdent>().is_ok());
//! assert!("_identifier".ck::<RustIdent>().is_ok());
//! assert!("Москва".ck::<RustIdent>().is_ok());
//! assert!("東京".ck::<RustIdent>().is_ok());
//!
//! assert!("struct".ck::<RustIdent>().is_err());
//! assert!("r#try".ck::<RustIdent>().is_err());
//! assert!("👍".ck::<RustIdent>().is_err());
//! ```
//!
//! # Aliases
//!
//! This module exposes [`Ident`] and [`IdentBuf`], which alias `Ck<RustIdent>`
//! and `Check<RustIdent>` respectively. These aliases are preferred to keep
//! type signatures succinct.
//!
//! # Requirements
//!
//! This module is only available when the `rust` feature flag is enabled.
use crate::{ident::unicode, Check, Ck, Invariant};
use core::fmt;

/// An [`Invariant`] for Rust identifiers.
///
/// Raw identifiers are unsupported.
///
/// # Invariants
///
/// * The string is nonempty.
/// * The first character is `_` or XID_Start.
/// * Any following characters are XID_Continue.
/// * The string isn't a single underscore, e.g. `"_"`.
/// * The string isn't a [strict] or [reserved] keyword.
///
/// [strict]: https://doc.rust-lang.org/reference/keywords.html#strict-keywords
/// [reserved]: https://doc.rust-lang.org/reference/keywords.html#reserved-keywords
#[derive(Clone, Debug)]
pub struct RustIdent;

/// Borrowed checked string containing a Rust identifier.
///
/// See [`RustIdent`] for more details.
pub type Ident = Ck<RustIdent>;

/// Owned checked string containing a Rust identifier.
///
/// See [`RustIdent`] for more details.
pub type IdentBuf<B = String> = Check<RustIdent, B>;

/// The error type returned from checking the invariants of [`RustIdent`].
#[derive(Debug, Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum Error {
    /// An invalid unicode identifier.
    Unicode(unicode::Error),

    /// A [strict] or [reserved] keyword.
    ///
    /// [strict]: https://doc.rust-lang.org/reference/keywords.html#strict-keywords
    /// [reserved]: https://doc.rust-lang.org/reference/keywords.html#reserved-keywords
    Keyword(&'static str),

    /// A single underscore.
    Wildcard,
}

impl std::error::Error for Error {}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::Unicode(err) => err.fmt(f),
            Error::Keyword(keyword) => {
                write!(f, "Rust keyword: '{keyword}'")
            }
            Error::Wildcard => f.pad("wildcard '_' isn't a valid Rust ident"),
        }
    }
}

impl Invariant for RustIdent {
    type Error = Error;

    fn check(slice: &str) -> Result<(), Self::Error> {
        match unicode::UnicodeIdent::check(slice) {
            Ok(()) => match KEYWORDS.binary_search(&slice) {
                Ok(index) => Err(Error::Keyword(KEYWORDS[index])),
                Err(_) => Ok(()),
            },
            Err(unicode::Error::Start('_')) => match slice.len() {
                1 => Err(Error::Wildcard), // `_` isn't ok
                _ => Ok(()),               // `_x` is ok
            },
            Err(e) => Err(Error::Unicode(e)),
        }
    }
}

static KEYWORDS: [&str; 51] = [
    "Self", "abstract", "as", "async", "await", "become", "box", "break", "const", "continue",
    "crate", "do", "dyn", "else", "enum", "extern", "false", "final", "fn", "for", "if", "impl",
    "in", "let", "loop", "macro", "match", "mod", "move", "mut", "override", "priv", "pub", "ref",
    "return", "self", "static", "struct", "super", "trait", "true", "try", "type", "typeof",
    "unsafe", "unsized", "use", "virtual", "where", "while", "yield",
];

#[cfg(test)]
mod tests {
    use super::{Error, RustIdent};
    use crate::IntoCk;

    #[test]
    fn test_underscore() {
        assert_eq!("_".ck::<RustIdent>().unwrap_err(), Error::Wildcard);
        assert!("_unused".ck::<RustIdent>().is_ok());
        assert!("__private".ck::<RustIdent>().is_ok());
        assert!("snake_case".ck::<RustIdent>().is_ok());
    }

    #[test]
    fn test_rust_reference() {
        assert!("foo".ck::<RustIdent>().is_ok());
        assert!("_identifier".ck::<RustIdent>().is_ok());
        assert!("Москва".ck::<RustIdent>().is_ok());
        assert!("東京".ck::<RustIdent>().is_ok());
    }

    #[test]
    fn rust_keywords_fail() {
        assert!("return".ck::<RustIdent>().is_err());
        assert!("continued".ck::<RustIdent>().is_ok());
    }
}
