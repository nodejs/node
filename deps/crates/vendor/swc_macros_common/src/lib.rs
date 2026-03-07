//! Internal crate for the swc project.

extern crate proc_macro;

use proc_macro2::{Span, TokenStream};
use quote::ToTokens;
use syn::{punctuated::Pair, *};

pub mod binder;
pub mod derive;
pub mod prelude;
mod syn_ext;

pub fn call_site() -> Span {
    Span::call_site()
}

pub fn def_site() -> Span {
    call_site()
}

/// `attr` - tokens inside `#[]`. e.g. `derive(EqIgnoreSpan)`, ast_node
pub fn print(attr: &'static str, tokens: proc_macro2::TokenStream) -> proc_macro::TokenStream {
    use std::env;

    match env::var("PRINT_GENERATED") {
        Ok(ref s) if s == "1" || attr == s => {}
        _ => return tokens.into(),
    }

    println!("\n\tOutput of #[{attr}]:\n\t {tokens}");
    tokens.into()
}

pub fn is_attr_name(attr: &Attribute, name: &str) -> bool {
    attr.path().is_ident(name)
}

/// Returns `None` if `attr` is not a doc attribute.
pub fn doc_str(attr: &Attribute) -> Option<String> {
    fn parse_tts(attr: &Attribute) -> String {
        match &attr.meta {
            Meta::NameValue(MetaNameValue {
                value:
                    Expr::Lit(ExprLit {
                        lit: Lit::Str(s), ..
                    }),
                ..
            }) => s.value(),
            _ => panic!("failed to parse {:?}", attr.meta),
        }
    }

    if !is_attr_name(attr, "doc") {
        return None;
    }

    Some(parse_tts(attr))
}

/// Creates a doc comment.
pub fn make_doc_attr(s: &str) -> Attribute {
    let span = Span::call_site();

    Attribute {
        style: AttrStyle::Outer,
        bracket_token: Default::default(),
        pound_token: Token![#](span),
        meta: Meta::NameValue(MetaNameValue {
            path: Path {
                leading_colon: None,
                segments: vec![Pair::End(PathSegment {
                    ident: Ident::new("doc", span),
                    arguments: Default::default(),
                })]
                .into_iter()
                .collect(),
            },
            eq_token: Token![=](span),
            value: Expr::Lit(ExprLit {
                attrs: Default::default(),
                lit: Lit::Str(LitStr::new(s, span)),
            }),
        }),
    }
}

pub fn access_field(obj: &dyn ToTokens, idx: usize, f: &Field) -> Expr {
    Expr::Field(ExprField {
        attrs: Default::default(),
        base: syn::parse2(obj.to_token_stream())
            .expect("swc_macros_common::access_field: failed to parse object"),
        dot_token: Token![.](Span::call_site()),
        member: match &f.ident {
            Some(id) => Member::Named(id.clone()),
            _ => Member::Unnamed(Index {
                index: idx as _,
                span: Span::call_site(),
            }),
        },
    })
}

pub fn join_stmts(stmts: &[Stmt]) -> TokenStream {
    let mut q = TokenStream::new();

    for s in stmts {
        s.to_tokens(&mut q);
    }

    q
}

/// fail! is a panic! with location reporting.
#[macro_export]
macro_rules! fail {
    ($($args:tt)+) => {{
        panic!("{}\n --> {}:{}:{}", format_args!($($args)*), file!(), line!(), column!());
    }};
}

#[macro_export]
macro_rules! unimplemented {
    ($($args:tt)+) => {{
        fail!("not yet implemented: {}", format_args!($($args)*));
    }};
}

#[macro_export]
macro_rules! unreachable {
    () => {{
        fail!("internal error: unreachable");
    }};
    ($($args:tt)+) => {{
        fail!("internal error: unreachable\n{}", format_args!($($args)*));
    }};
}
