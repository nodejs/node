use quote::{ToTokens, TokenStreamExt};
use serde::{Deserialize, Serialize};
use std::borrow::{Borrow, Cow};
use std::fmt;
use std::ops::Range;

/// Equivalent to `proc_macro2::LineColumn`
#[derive(Hash, Eq, PartialEq, Serialize, Clone, Debug)]
pub struct LineColumn {
    /// The 1-indexed row of where we are in a file
    /// (Alternatively, the offset by the number of newline characters + 1)
    pub line: usize,
    /// The 0-indexed column of where we are in a file
    /// (Alternatively, the offset by newline characters plus this amount)
    pub col: usize,
}

/// Equivalent to `proc_macro2::Span`.
#[derive(Hash, Eq, PartialEq, Serialize, Clone, Debug)]
pub struct Span {
    pub start: LineColumn,
    pub end: LineColumn,
    /// The range in bytes of the span. Used mostly to determine length.
    pub range: Range<usize>,
    /// Equivalent to `proc_macro2::Span::file`, but we have to allow for internal testing or
    /// spaces where we simply don't know where we've acquired this Span from.
    pub span_location: SpanLocation,
}

#[derive(Hash, Eq, PartialEq, Serialize, Clone, Debug)]
#[non_exhaustive]
pub enum SpanLocation {
    /// For testing or when accessing SpanLocation is not possible.
    /// Will render as <location unknown> on rendering an error.
    None,
    /// An absolute path to a rust file.
    FilePath(String),
    /// Does not exist in a file, full source text that is used locally.
    LocalSource(String),
}

/// An identifier, analogous to `syn::Ident` and `proc_macro2::Ident`.
#[derive(Eq, Serialize, Clone, Debug)]
pub struct Ident(Cow<'static, str>, Option<Span>);

impl Ident {
    /// Validate a string
    fn validate(string: &str) -> syn::Result<()> {
        syn::parse_str::<syn::Ident>(string).map(|_| {})
    }

    pub fn to_syn(&self) -> syn::Ident {
        syn::Ident::new(self.as_str(), proc_macro2::Span::call_site())
    }

    /// Get the `&str` representation.
    pub fn as_str(&self) -> &str {
        &self.0
    }

    pub fn span(&self) -> Option<Span> {
        self.1.clone()
    }

    /// An [`Ident`] containing "this".
    pub const THIS: Self = Ident(Cow::Borrowed("this"), None);
}

/// Helper
pub(crate) trait FromWithSpan<T>: Sized {
    fn spanned_from(value: T, module_location: &SpanLocation) -> Self;
}

pub(crate) trait IntoWithSpan<T>: Sized {
    fn spanned_into(self, module_location: &SpanLocation) -> T;
}

impl<T, U> IntoWithSpan<U> for T
where
    U: FromWithSpan<T>,
{
    fn spanned_into(self, module_location: &SpanLocation) -> U {
        FromWithSpan::spanned_from(self, module_location)
    }
}

impl std::hash::Hash for Ident {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.0.hash(state);
    }
}

impl Ord for Ident {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.0.cmp(&other.0)
    }
}

impl PartialOrd for Ident {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for Ident {
    fn eq(&self, other: &Self) -> bool {
        self.0.eq(&other.0)
    }
}

impl From<&'static str> for Ident {
    fn from(string: &'static str) -> Self {
        Self::validate(string).unwrap();
        Self(Cow::from(string), None)
    }
}

impl From<String> for Ident {
    fn from(string: String) -> Self {
        Self::validate(&string).unwrap();
        Self(Cow::from(string), None)
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

impl FromWithSpan<&syn::Ident> for Ident {
    fn spanned_from(ident: &syn::Ident, span_location: &SpanLocation) -> Self {
        Self(
            Cow::from(ident.to_string()),
            Some(ident.span().spanned_into(span_location)),
        )
    }
}

impl FromWithSpan<proc_macro2::Span> for Span {
    fn spanned_from(span: proc_macro2::Span, span_location: &SpanLocation) -> Self {
        let start = span.start();
        let end = span.end();
        Span {
            start: LineColumn {
                line: start.line,
                col: start.column,
            },
            end: LineColumn {
                line: end.line,
                col: end.column,
            },
            range: span.byte_range(),
            span_location: span_location.clone(),
        }
    }
}

impl ToTokens for Ident {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        tokens.append(self.to_syn());
    }
}

#[cfg(test)]
mod tests {
    use crate::ast::Module;

    #[test]
    fn test_span_parsing() {
        let crate_dir = env!("CARGO_MANIFEST_DIR");
        let file = std::fs::read_to_string(
            std::path::Path::new(crate_dir).join("src/ast/snapshots/span_testing.txt"),
        )
        .expect("Could not read file");
        let f = syn::parse_str::<syn::ItemMod>(&file);
        let inner = match f {
            Ok(i) => i,
            Err(e) => {
                let span = e.span();
                panic!(
                    "File parsing error: {e:?} ./snapshots/span_testing.txt:{}:{}",
                    span.start().line,
                    span.start().column
                )
            }
        };
        let file = Module::from_syn(
            &inner,
            true,
            None,
            &super::SpanLocation::FilePath("./snapshots/span_testing.txt".into()),
        );
        insta::assert_yaml_snapshot!(file);
    }
}
