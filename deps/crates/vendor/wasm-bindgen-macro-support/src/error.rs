use proc_macro2::*;
use quote::{ToTokens, TokenStreamExt};
use syn::parse::Error;

/// Provide a Diagnostic with the given span and message
macro_rules! err_span {
    ($span:expr, $($msg:tt)*) => (
        $crate::Diagnostic::spanned_error(&$span, format!($($msg)*))
    )
}

/// Immediately fail and return an Err, with the arguments passed to err_span!
macro_rules! bail_span {
    ($($t:tt)*) => (
        return Err(err_span!($($t)*).into())
    )
}

/// A struct representing a diagnostic to emit to the end-user as an error.
#[derive(Debug)]
pub struct Diagnostic {
    inner: Repr,
}

#[derive(Debug)]
enum Repr {
    Single {
        text: String,
        span: Option<(Span, Span)>,
    },
    SynError(Error),
    Multi {
        diagnostics: Vec<Diagnostic>,
    },
}

impl Diagnostic {
    /// Generate a `Diagnostic` from an informational message with no Span
    pub fn error<T: Into<String>>(text: T) -> Diagnostic {
        Diagnostic {
            inner: Repr::Single {
                text: text.into(),
                span: None,
            },
        }
    }

    /// Generate a `Diagnostic` from a Span and an informational message
    pub fn span_error<T: Into<String>>(span: Span, text: T) -> Diagnostic {
        Diagnostic {
            inner: Repr::Single {
                text: text.into(),
                span: Some((span, span)),
            },
        }
    }

    /// Generate a `Diagnostic` from the span of any tokenizable object and a message
    pub fn spanned_error<T: Into<String>>(node: &dyn ToTokens, text: T) -> Diagnostic {
        Diagnostic {
            inner: Repr::Single {
                text: text.into(),
                span: extract_spans(node),
            },
        }
    }

    /// Attempt to generate a `Diagnostic` from a vector of other `Diagnostic` instances.
    /// If the `Vec` is empty, returns `Ok(())`, otherwise returns the new `Diagnostic`
    pub fn from_vec(diagnostics: Vec<Diagnostic>) -> Result<(), Diagnostic> {
        if diagnostics.is_empty() {
            Ok(())
        } else {
            Err(Diagnostic {
                inner: Repr::Multi { diagnostics },
            })
        }
    }

    /// Immediately trigger a panic from this `Diagnostic`
    #[allow(unconditional_recursion)]
    pub fn panic(&self) -> ! {
        match &self.inner {
            Repr::Single { text, .. } => panic!("{}", text),
            Repr::SynError(error) => panic!("{}", error),
            Repr::Multi { diagnostics } => diagnostics[0].panic(),
        }
    }
}

impl From<Error> for Diagnostic {
    fn from(err: Error) -> Diagnostic {
        Diagnostic {
            inner: Repr::SynError(err),
        }
    }
}

fn extract_spans(node: &dyn ToTokens) -> Option<(Span, Span)> {
    let mut t = TokenStream::new();
    node.to_tokens(&mut t);
    let mut tokens = t.into_iter();
    let start = tokens.next().map(|t| t.span());
    let end = tokens.last().map(|t| t.span());
    start.map(|start| (start, end.unwrap_or(start)))
}

impl ToTokens for Diagnostic {
    fn to_tokens(&self, dst: &mut TokenStream) {
        match &self.inner {
            Repr::Single { text, span } => {
                let cs2 = (Span::call_site(), Span::call_site());
                let (start, end) = span.unwrap_or(cs2);
                dst.append(Ident::new("compile_error", start));
                dst.append(Punct::new('!', Spacing::Alone));
                let mut message = TokenStream::new();
                message.append(Literal::string(text));
                let mut group = Group::new(Delimiter::Brace, message);
                group.set_span(end);
                dst.append(group);
            }
            Repr::Multi { diagnostics } => {
                for diagnostic in diagnostics {
                    diagnostic.to_tokens(dst);
                }
            }
            Repr::SynError(err) => {
                err.to_compile_error().to_tokens(dst);
            }
        }
    }
}
