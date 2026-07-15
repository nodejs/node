use super::ToTokens;
use core::iter;
use proc_macro2::{TokenStream, TokenTree};

/// TokenStream extension trait with methods for appending tokens.
///
/// This trait is sealed and cannot be implemented outside of the `quote` crate.
pub trait TokenStreamExt: private::Sealed {
    /// For use by `ToTokens` implementations.
    ///
    /// Appends the token specified to this list of tokens.
    fn append<U>(&mut self, token: U)
    where
        U: Into<TokenTree>;

    /// For use by `ToTokens` implementations.
    ///
    /// ```
    /// # use quote::{quote, TokenStreamExt, ToTokens};
    /// # use proc_macro2::TokenStream;
    /// #
    /// struct X;
    ///
    /// impl ToTokens for X {
    ///     fn to_tokens(&self, tokens: &mut TokenStream) {
    ///         tokens.append_all(&[true, false]);
    ///     }
    /// }
    ///
    /// let tokens = quote!(#X);
    /// assert_eq!(tokens.to_string(), "true false");
    /// ```
    fn append_all<I>(&mut self, iter: I)
    where
        I: IntoIterator,
        I::Item: ToTokens;

    /// For use by `ToTokens` implementations.
    ///
    /// Appends all of the items in the iterator `I`, separated by the tokens
    /// `U`.
    fn append_separated<I, U>(&mut self, iter: I, op: U)
    where
        I: IntoIterator,
        I::Item: ToTokens,
        U: ToTokens;

    /// For use by `ToTokens` implementations.
    ///
    /// Appends all tokens in the iterator `I`, appending `U` after each
    /// element, including after the last element of the iterator.
    fn append_terminated<I, U>(&mut self, iter: I, term: U)
    where
        I: IntoIterator,
        I::Item: ToTokens,
        U: ToTokens;
}

impl TokenStreamExt for TokenStream {
    fn append<U>(&mut self, token: U)
    where
        U: Into<TokenTree>,
    {
        self.extend(iter::once(token.into()));
    }

    fn append_all<I>(&mut self, iter: I)
    where
        I: IntoIterator,
        I::Item: ToTokens,
    {
        do_append_all(self, iter.into_iter());

        fn do_append_all<I>(stream: &mut TokenStream, iter: I)
        where
            I: Iterator,
            I::Item: ToTokens,
        {
            for token in iter {
                token.to_tokens(stream);
            }
        }
    }

    fn append_separated<I, U>(&mut self, iter: I, op: U)
    where
        I: IntoIterator,
        I::Item: ToTokens,
        U: ToTokens,
    {
        do_append_separated(self, iter.into_iter(), op);

        fn do_append_separated<I, U>(stream: &mut TokenStream, iter: I, op: U)
        where
            I: Iterator,
            I::Item: ToTokens,
            U: ToTokens,
        {
            let mut first = true;
            for token in iter {
                if !first {
                    op.to_tokens(stream);
                }
                first = false;
                token.to_tokens(stream);
            }
        }
    }

    fn append_terminated<I, U>(&mut self, iter: I, term: U)
    where
        I: IntoIterator,
        I::Item: ToTokens,
        U: ToTokens,
    {
        do_append_terminated(self, iter.into_iter(), term);

        fn do_append_terminated<I, U>(stream: &mut TokenStream, iter: I, term: U)
        where
            I: Iterator,
            I::Item: ToTokens,
            U: ToTokens,
        {
            for token in iter {
                token.to_tokens(stream);
                term.to_tokens(stream);
            }
        }
    }
}

mod private {
    use proc_macro2::TokenStream;

    pub trait Sealed {}

    impl Sealed for TokenStream {}
}
