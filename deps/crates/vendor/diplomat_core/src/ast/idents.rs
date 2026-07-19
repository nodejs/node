use proc_macro2::Span;
use quote::{ToTokens, TokenStreamExt};
use serde::{Deserialize, Serialize};
use std::borrow::{Borrow, Cow};
use std::fmt;

/// An identifier, analogous to `syn::Ident` and `proc_macro2::Ident`.
#[derive(Hash, Eq, PartialEq, Serialize, Clone, Debug, Ord, PartialOrd)]
#[serde(transparent)]
pub struct Ident(Cow<'static, str>);

impl Ident {
    /// Validate a string
    fn validate(string: &str) -> syn::Result<()> {
        syn::parse_str::<syn::Ident>(string).map(|_| {})
    }

    /// Attempt to create a new `Ident`.
    ///
    /// This function fails if the input isn't valid according to
    /// `proc_macro2::Ident`'s invariants.
    pub fn try_new(string: String) -> syn::Result<Self> {
        Self::validate(&string).map(|_| Self(Cow::from(string)))
    }

    pub fn to_syn(&self) -> syn::Ident {
        syn::Ident::new(self.as_str(), Span::call_site())
    }

    /// Get the `&str` representation.
    pub fn as_str(&self) -> &str {
        &self.0
    }

    /// An [`Ident`] containing "this".
    pub const THIS: Self = Ident(Cow::Borrowed("this"));
}

impl From<&'static str> for Ident {
    fn from(string: &'static str) -> Self {
        Self::validate(string).unwrap();
        Self(Cow::from(string))
    }
}

impl From<String> for Ident {
    fn from(string: String) -> Self {
        Self::validate(&string).unwrap();
        Self(Cow::from(string))
    }
}

impl<'de> Deserialize<'de> for Ident {
    /// The derived `Deserialize` allows for creating `Ident`s that do not uphold
    /// the proper invariants. This custom impl ensures that this cannot happen.
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        Ok(Ident::from(String::deserialize(deserializer)?))
    }
}

impl Borrow<str> for Ident {
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

impl fmt::Display for Ident {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.as_str().fmt(f)
    }
}

impl From<&syn::Ident> for Ident {
    fn from(ident: &syn::Ident) -> Self {
        Self(Cow::from(ident.to_string()))
    }
}

impl ToTokens for Ident {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        tokens.append(self.to_syn());
    }
}
