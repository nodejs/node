//! Extension traits to provide parsing methods on foreign types.

#[cfg(feature = "parsing")]
use crate::buffer::Cursor;
#[cfg(feature = "parsing")]
use crate::error::Result;
#[cfg(feature = "parsing")]
use crate::parse::ParseStream;
#[cfg(feature = "parsing")]
use crate::parse::Peek;
#[cfg(feature = "parsing")]
use crate::sealed::lookahead;
#[cfg(feature = "parsing")]
use crate::token::CustomToken;
use proc_macro2::{Ident, Punct, Spacing, Span, TokenStream, TokenTree};
use std::iter;

/// Additional methods for `Ident` not provided by proc-macro2 or libproc_macro.
///
/// This trait is sealed and cannot be implemented for types outside of Syn. It
/// is implemented only for `proc_macro2::Ident`.
pub trait IdentExt: Sized + private::Sealed {
    /// Parses any identifier including keywords.
    ///
    /// This is useful when parsing macro input which allows Rust keywords as
    /// identifiers.
    ///
    /// # Example
    ///
    /// ```
    /// use syn::{Error, Ident, Result, Token};
    /// use syn::ext::IdentExt;
    /// use syn::parse::ParseStream;
    ///
    /// mod kw {
    ///     syn::custom_keyword!(name);
    /// }
    ///
    /// // Parses input that looks like `name = NAME` where `NAME` can be
    /// // any identifier.
    /// //
    /// // Examples:
    /// //
    /// //     name = anything
    /// //     name = impl
    /// fn parse_dsl(input: ParseStream) -> Result<Ident> {
    ///     input.parse::<kw::name>()?;
    ///     input.parse::<Token![=]>()?;
    ///     let name = input.call(Ident::parse_any)?;
    ///     Ok(name)
    /// }
    /// ```
    #[cfg(feature = "parsing")]
    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    fn parse_any(input: ParseStream) -> Result<Self>;

    /// Peeks any identifier including keywords. Usage:
    /// `input.peek(Ident::peek_any)`
    ///
    /// This is different from `input.peek(Ident)` which only returns true in
    /// the case of an ident which is not a Rust keyword.
    #[cfg(feature = "parsing")]
    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    #[allow(non_upper_case_globals)]
    const peek_any: private::PeekFn = private::PeekFn;

    /// Strips the raw marker `r#`, if any, from the beginning of an ident.
    ///
    ///   - unraw(`x`) = `x`
    ///   - unraw(`move`) = `move`
    ///   - unraw(`r#move`) = `move`
    ///
    /// # Example
    ///
    /// In the case of interop with other languages like Python that have a
    /// different set of keywords than Rust, we might come across macro input
    /// that involves raw identifiers to refer to ordinary variables in the
    /// other language with a name that happens to be a Rust keyword.
    ///
    /// The function below appends an identifier from the caller's input onto a
    /// fixed prefix. Without using `unraw()`, this would tend to produce
    /// invalid identifiers like `__pyo3_get_r#move`.
    ///
    /// ```
    /// use proc_macro2::Span;
    /// use syn::Ident;
    /// use syn::ext::IdentExt;
    ///
    /// fn ident_for_getter(variable: &Ident) -> Ident {
    ///     let getter = format!("__pyo3_get_{}", variable.unraw());
    ///     Ident::new(&getter, Span::call_site())
    /// }
    /// ```
    fn unraw(&self) -> Ident;
}

impl IdentExt for Ident {
    #[cfg(feature = "parsing")]
    fn parse_any(input: ParseStream) -> Result<Self> {
        input.step(|cursor| match cursor.ident() {
            Some((ident, rest)) => Ok((ident, rest)),
            None => Err(cursor.error("expected ident")),
        })
    }

    fn unraw(&self) -> Ident {
        let string = self.to_string();
        if let Some(string) = string.strip_prefix("r#") {
            Ident::new(string, self.span())
        } else {
            self.clone()
        }
    }
}

#[cfg(feature = "parsing")]
impl Peek for private::PeekFn {
    type Token = private::IdentAny;
}

#[cfg(feature = "parsing")]
impl CustomToken for private::IdentAny {
    fn peek(cursor: Cursor) -> bool {
        cursor.ident().is_some()
    }

    fn display() -> &'static str {
        "identifier"
    }
}

#[cfg(feature = "parsing")]
impl lookahead::Sealed for private::PeekFn {}

pub(crate) trait TokenStreamExt {
    fn append(&mut self, token: TokenTree);
}

impl TokenStreamExt for TokenStream {
    fn append(&mut self, token: TokenTree) {
        self.extend(iter::once(token));
    }
}

pub(crate) trait PunctExt {
    fn new_spanned(ch: char, spacing: Spacing, span: Span) -> Self;
}

impl PunctExt for Punct {
    fn new_spanned(ch: char, spacing: Spacing, span: Span) -> Self {
        let mut punct = Punct::new(ch, spacing);
        punct.set_span(span);
        punct
    }
}

mod private {
    use proc_macro2::Ident;

    pub trait Sealed {}

    impl Sealed for Ident {}

    #[cfg(feature = "parsing")]
    pub struct PeekFn;

    #[cfg(feature = "parsing")]
    pub struct IdentAny;

    #[cfg(feature = "parsing")]
    impl Copy for PeekFn {}

    #[cfg(feature = "parsing")]
    impl Clone for PeekFn {
        fn clone(&self) -> Self {
            *self
        }
    }
}
