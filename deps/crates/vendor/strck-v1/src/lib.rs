//! [![github-img]][github-url] [![crates-img]][crates-url] [![docs-img]][docs-url]
//!
//! [github-url]: https://github.com/QnnOkabayashi/strck
//! [crates-url]: https://crates.io/crates/strck
//! [docs-url]: crate
//! [github-img]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-img]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-img]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
//!
//! Checked owned and borrowed strings.
//!
//! # Overview
//!
//! The Rust standard library provides the `String` and `str` types, which wrap
//! `Vec<u8>` and `[u8]` respectively, with the invariant that the contents
//! are valid UTF-8.
//!
//! This crate abstracts the idea of type-level invariants on strings by
//! introducing the immutable [`Check`] and [`Ck`] types, where the invariants
//! are determined by a generic [`Invariant`] type parameter. It offers
//! [`UnicodeIdent`](crate::ident::unicode::UnicodeIdent)
//! and [`RustIdent`](crate::ident::rust::RustIdent) [`Invariant`]s,
//! which are enabled by the `ident` feature flag.
//!
//! "strck" comes from "str check", similar to how rustc has typeck and
//! borrowck for type check and borrow check respectively.
//!
//! # Motivation
//!
//! Libraries working with string-like types with certain properties, like identifiers,
//! quickly become confusing as `&str` and `String` begin to pollute type signatures
//! everywhere. One solution is to manually implement an owned checked string type
//! like [`syn::Ident`] to disambiguate the type signatures and validate the string.
//! The downside is that new values cannot be created without allocation,
//! which is unnecessary when only a borrowed version is required.
//!
//! `strck` solves this issue by providing a checked borrowed string type, [`Ck`],
//! alongside a checked owned string type, [`Check`]. These serve as thin wrappers
//! around `str` and `String`[^1] respectively, and prove at the type level that
//! the contents satisfy the [`Invariant`] that the wrapper is generic over.
//!
//! [^1]: [`Check`] can actually be backed by any `'static + AsRef<str>` type,
//! but `String` is the default.
//!
//! # Use cases
//!
//! ### Checked strings without allocating
//!
//! The main benefit `strck` offers is validating borrowed strings via the
//! [`Ck`] type without having to allocate in the result.
//!
//! ```rust
//! use strck::{Ck, IntoCk, ident::rust::RustIdent};
//!
//! let this_ident: &Ck<RustIdent> = "this".ck().unwrap();
//! ```
//!
//! ### Checked zero-copy deserialization
//!
//! When the `serde` feature flag is enabled, [`Ck`]s can be used to perform
//! checked zero-copy deserialization, which requires the
//! [`#[serde(borrow)]`][borrow] attribute.
//!
//! ```rust
//! # use serde::{Serialize, Deserialize};
//! use strck::{Ck, ident::unicode::UnicodeIdent};
//!
//! #[derive(Serialize, Deserialize)]
//! struct Player<'a> {
//!     #[serde(borrow)]
//!     username: &'a Ck<UnicodeIdent>,
//!     level: u32,
//! }
//! ```
//!
//! Note that this code sample explicitly uses `Ck<UnicodeIdent>` to demonstrate
//! that the type is a [`Ck`]. However, `strck` provides [`Ident`] as an
//! alias for `Ck<UnicodeIdent>`, which should be used in practice.
//!
//! ### Infallible parsing
//!
//! For types where string validation is relatively cheap but parsing is costly
//! and fallible, `strck` can be used with a custom [`Invariant`] as an input to
//! make an infallible parsing function.
//!
//! # Postfix construction with `IntoCk` and `IntoCheck`
//!
//! This crate exposes two helper traits, [`IntoCk`] and [`IntoCheck`]. When in
//! scope, the [`.ck()`] and [`.check()`] functions can be used to create
//! [`Ck`]s and [`Check`]s respectively:
//!
//! ```rust
//! use strck::{IntoCheck, IntoCk, ident::unicode::UnicodeIdent};
//!
//! let this_ident = "this".ck::<UnicodeIdent>().unwrap();
//! let this_foo_ident = format!("{}_foo", this_ident).check::<UnicodeIdent>().unwrap();
//! ```
//!
//! # Feature flags
//!
//! * `serde`: Implements `Serialize`/`Deserialize` for [`Check`]s and [`Ck`]s,
//! where the invariants are checked during deserialization. Disabled by default.
//!
//! [`syn::Ident`]: https://docs.rs/syn/latest/syn/struct.Ident.html
//! [`Ident`]: https://docs.rs/strck/latest/strck/ident/unicode/type.Ident.html
//! [borrow]: https://serde.rs/lifetimes.html#borrowing-data-in-a-derived-impl
//! [`.ck()`]: IntoCk::ck
//! [`.check()`]: IntoCheck::check
use core::{borrow, cmp, fmt, hash, marker, ops, str};

#[cfg(feature = "ident")]
pub mod ident;
mod partial_eq;
#[cfg(feature = "serde")]
mod serde;

/// Owned immutable string with invariants.
///
/// Similar to how `String` derefs to `&str`, [`Check`] derefs to [`&Ck`](Ck).
/// This means APIs requiring `&Check<I>` as an argument should instead consider
/// accepting `&Ck<I>` for more flexibility.
///
/// # Buffers
///
/// By default, this type is backed by a `String`, but it can also be backed by
/// any `AsRef<str> + 'static` type. In particular, types like [`SmolStr`] are
/// good candidates since they're designed to be immutable.
///
/// It's recommended to use a type alias when using a custom backing type, since
/// extra generics can make the type signature long.
///
/// [`SmolStr`]: https://docs.rs/smol_str/latest/smol_str/struct.SmolStr.html
#[derive(Clone)]
#[repr(transparent)]
pub struct Check<I: Invariant, B: AsRef<str> + 'static = String> {
    _marker: marker::PhantomData<I>,
    buf: B,
}

/// Borrowed immutable string with invariants.
///
/// [`Ck`] is a DST, and therefore must always live behind a pointer. This means
/// you'll usually see it as `&Ck<I>` in type signatures.
///
/// # Deserialization
///
/// See the [crate-level documentation] for details on how to use [`Ck`] for
/// checked zero-copy deserialization.
///
/// [crate-level documentation]: crate#checked-zero-copy-deserialization
#[repr(transparent)]
pub struct Ck<I: Invariant> {
    _marker: marker::PhantomData<I>,
    slice: str,
}

/// Invariant for a [`Ck`] or [`Check`].
///
/// The [`Ck`] and [`Check`] types are checked strings types that make guarantees
/// about the contents of the string. These guarantees are determined by this
/// trait, `Invariant` which distinguishes whether or not a string upholds some
/// arbitrary invariants via the [`Invariant::check`] function. If the `Err` is
/// returned, then the invariant is broken, and the `Ck` or `Check` generic over
/// the invariant cannot be constructed.
///
/// # Examples
///
/// Declaring an invariant that the string contains no whitespace:
/// ```rust
/// # use strck::Invariant;
/// struct NoWhitespace;
///
/// impl Invariant for NoWhitespace {
///     type Error = char;
///
///     fn check(slice: &str) -> Result<(), Self::Error> {
///         match slice.chars().find(|ch| ch.is_whitespace()) {
///             Some(ch) => Err(ch),
///             None => Ok(()),
///         }
///     }
/// }
/// ```
pub trait Invariant: Sized {
    /// The type returned in the event that an invariant is broken.
    ///
    /// When formatting, `Error` should not be capitalized and should not end
    /// with a period.
    type Error: fmt::Display;

    /// Returns `Ok` if the string upholds the invariant, otherwise `Err`.
    ///
    /// This function is used internally in [`Check::from_buf`] and [`Ck::from_slice`].
    fn check(slice: &str) -> Result<(), Self::Error>;
}

/// Conversion into a [`Ck`].
pub trait IntoCk: Sized + AsRef<str> {
    /// Returns a validated [`Ck`] borrowing from `self`.
    ///
    /// # Examples
    ///
    /// Creating an Rust ident containing `this`:
    /// ```rust
    /// use strck::{IntoCk, ident::rust::Ident};
    ///
    /// let this_ident: &Ident = "this".ck().unwrap();
    /// ```
    fn ck<I: Invariant>(&self) -> Result<&Ck<I>, I::Error>;
}

impl<T: AsRef<str>> IntoCk for T {
    fn ck<I: Invariant>(&self) -> Result<&Ck<I>, I::Error> {
        Ck::from_slice(self.as_ref())
    }
}

/// Conversion into a [`Check`].
pub trait IntoCheck: Sized + AsRef<str> + 'static {
    /// Returns a validated [`Check`] owning `self`.
    ///
    /// Note that [`Check`] uses the input of [`IntoCheck::check`] as its backing
    /// storage, meaning that `"this".check()` will return a `Check<I, &'static str>`.
    /// Although this is technically valid, it's _strongly_ recommended to use
    /// [`Ck`] for string slices instead to avoid confusion.
    ///
    /// # Examples
    ///
    /// Creating a Unicode ident from a formatted string:
    /// ```rust
    /// use strck::{Check, Ck, IntoCheck, ident::unicode::UnicodeIdent};
    ///
    /// fn wrapper_name(name: &Ck<UnicodeIdent>) -> Check<UnicodeIdent> {
    ///     format!("lil_{name}").check().unwrap()
    /// }
    /// ```
    fn check<I: Invariant>(self) -> Result<Check<I, Self>, I::Error>;
}

impl<T: AsRef<str> + 'static> IntoCheck for T {
    fn check<I: Invariant>(self) -> Result<Check<I, Self>, I::Error> {
        Check::from_buf(self)
    }
}

// impl Check

impl<I: Invariant, B: AsRef<str>> Check<I, B> {
    /// Returns an `Ok` if the buffer upholds the invariants, otherwise `Err`.
    pub fn from_buf(buf: B) -> Result<Self, I::Error> {
        I::check(buf.as_ref())?;

        Ok(Check {
            _marker: marker::PhantomData,
            buf,
        })
    }

    /// Returns a [`&Ck`](Ck) that borrows from `self`.
    pub fn as_ck(&self) -> &Ck<I> {
        // SAFETY: `self` has the same invariants as `&Ck<I>`, and `Ck` has the
        // same ABI as `str` by `#[repr(transparent)]`.
        unsafe { core::mem::transmute(self.buf.as_ref()) }
    }

    /// Returns the inner representation.
    pub fn into_inner(self) -> B {
        self.buf
    }
}

impl<I, B> fmt::Debug for Check<I, B>
where
    I: Invariant,
    B: AsRef<str> + fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&self.buf, f)
    }
}

impl<I, B1, B2> PartialEq<Check<I, B2>> for Check<I, B1>
where
    I: Invariant,
    B1: AsRef<str>,
    B2: AsRef<str>,
{
    fn eq(&self, other: &Check<I, B2>) -> bool {
        self.as_str() == other.as_str()
    }
}

impl<I, B1, B2> PartialOrd<Check<I, B2>> for Check<I, B1>
where
    I: Invariant,
    B1: AsRef<str>,
    B2: AsRef<str>,
{
    fn partial_cmp(&self, other: &Check<I, B2>) -> Option<cmp::Ordering> {
        self.as_ck().partial_cmp(other.as_ck())
    }
}

impl<I: Invariant, B: AsRef<str>> Eq for Check<I, B> {}

impl<I: Invariant, B: AsRef<str>> Ord for Check<I, B> {
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        self.as_ck().cmp(other.as_ck())
    }
}

impl<I: Invariant, B: AsRef<str>> hash::Hash for Check<I, B> {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.as_str().hash(state);
    }
}

impl<I: Invariant, B: AsRef<str>> ops::Deref for Check<I, B> {
    type Target = Ck<I>;

    fn deref(&self) -> &Self::Target {
        self.as_ck()
    }
}

impl<I: Invariant, B: AsRef<str>> AsRef<Ck<I>> for Check<I, B> {
    fn as_ref(&self) -> &Ck<I> {
        self.as_ck()
    }
}

impl<I: Invariant, B: AsRef<str>> AsRef<str> for Check<I, B> {
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl<I: Invariant, B: AsRef<str>> borrow::Borrow<Ck<I>> for Check<I, B> {
    fn borrow(&self) -> &Ck<I> {
        self.as_ck()
    }
}

impl<I: Invariant, B: AsRef<str>> fmt::Display for Check<I, B> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(self.as_str(), f)
    }
}

impl<'a, I, B> From<&'a Ck<I>> for Check<I, B>
where
    I: Invariant,
    B: AsRef<str> + From<&'a str>,
{
    fn from(check: &'a Ck<I>) -> Self {
        check.to_check()
    }
}

impl<I, B> str::FromStr for Check<I, B>
where
    I: Invariant,
    for<'a> B: AsRef<str> + From<&'a str>,
{
    type Err = I::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(s.ck()?.to_check())
    }
}

// impl Ck

impl<I: Invariant> Ck<I> {
    /// Returns an `Ok` if the `&str` upholds the invariants, otherwise `Err`.
    pub fn from_slice(slice: &str) -> Result<&Self, I::Error> {
        I::check(slice)?;

        // SAFETY: invariants are upheld, and `Ck` has the same ABI as `str` by `#[repr(transparent)]`.
        unsafe { Ok(core::mem::transmute::<&str, &Ck<I>>(slice)) }
    }

    /// Returns an owned [`Check`] from `&self`.
    pub fn to_check<'a, B>(&'a self) -> Check<I, B>
    where
        B: AsRef<str> + From<&'a str>,
    {
        Check {
            _marker: marker::PhantomData,
            buf: self.as_str().into(),
        }
    }

    /// Returns the `&str` representation.
    pub fn as_str(&self) -> &str {
        &self.slice
    }
}

impl<I: Invariant> fmt::Debug for Ck<I> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&self.slice, f)
    }
}

impl<I: Invariant> PartialEq for Ck<I> {
    fn eq(&self, other: &Self) -> bool {
        self.as_str() == other.as_str()
    }
}

impl<I: Invariant> PartialOrd for Ck<I> {
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<I: Invariant> Eq for Ck<I> {}

impl<I: Invariant> Ord for Ck<I> {
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        self.as_str().cmp(other.as_str())
    }
}

impl<I: Invariant> hash::Hash for Ck<I> {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.as_str().hash(state);
    }
}

impl<I: Invariant> AsRef<str> for Ck<I> {
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl<I: Invariant> borrow::Borrow<str> for Ck<I> {
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

impl<I: Invariant> ToOwned for Ck<I> {
    type Owned = Check<I>;

    fn to_owned(&self) -> Self::Owned {
        self.to_check()
    }
}

impl<I: Invariant> fmt::Display for Ck<I> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(self.as_str(), f)
    }
}

impl<'a, I: Invariant, B: AsRef<str>> From<&'a Check<I, B>> for &'a Ck<I> {
    fn from(check: &'a Check<I, B>) -> Self {
        check.as_ck()
    }
}

impl<'a, I: Invariant> TryFrom<&'a str> for &'a Ck<I> {
    type Error = I::Error;

    fn try_from(slice: &'a str) -> Result<Self, Self::Error> {
        Ck::from_slice(slice)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Test invariant.
    struct NoInvariant;

    impl Invariant for NoInvariant {
        type Error = core::convert::Infallible;

        fn check(_slice: &str) -> Result<(), Self::Error> {
            Ok(())
        }
    }

    #[test]
    fn test_debug_impl() {
        let this = "this".ck::<NoInvariant>().unwrap();
        let fmt_debug = format!("{this:?}");

        assert_eq!(fmt_debug, "\"this\"");
    }

    #[test]
    fn test_ck_partial_eq() {
        let this = "this".ck::<NoInvariant>().unwrap();
        let still_this = "this".ck::<NoInvariant>().unwrap();
        let other = "other".ck::<NoInvariant>().unwrap();

        // With other different instance
        assert_ne!(this, other);
        assert_ne!(this, &other);
        assert_ne!(&this, other);
        assert_ne!(&this, &other);

        // With other equal instance
        assert_eq!(this, still_this);
        assert_eq!(this, &still_this);
        assert_eq!(&this, still_this);
        assert_eq!(&this, &still_this);

        // With itself
        assert_eq!(this, this);
        assert_eq!(this, &this);
        assert_eq!(&this, this);
        assert_eq!(&this, &this);
    }

    #[test]
    fn test_check_partial_eq() {
        let this = "this".check::<NoInvariant>().unwrap();
        let still_this = "this".check::<NoInvariant>().unwrap();
        let other = "other".check::<NoInvariant>().unwrap();

        // With other different instance
        assert_ne!(this, other);
        assert_ne!(this, &other);
        assert_ne!(&this, other);
        assert_ne!(&this, &other);

        // With other equal instance
        assert_eq!(this, still_this);
        assert_eq!(this, &still_this);
        assert_eq!(&this, still_this);
        assert_eq!(&this, &still_this);

        // With itself
        assert_eq!(this, this);
        assert_eq!(this, &this);
        assert_eq!(&this, this);
        assert_eq!(&this, &this);
    }
}
