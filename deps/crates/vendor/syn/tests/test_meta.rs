#![allow(
    clippy::elidable_lifetime_names,
    clippy::needless_lifetimes,
    clippy::shadow_unrelated,
    clippy::too_many_lines,
    clippy::uninlined_format_args
)]

#[macro_use]
mod snapshot;

mod debug;

use quote::quote;
use syn::parse::{ParseStream, Parser as _, Result};
use syn::{Meta, MetaList, MetaNameValue, Token};

#[test]
fn test_parse_meta_item_word() {
    let input = "hello";

    snapshot!(input as Meta, @r#"
    Meta::Path {
        segments: [
            PathSegment {
                ident: "hello",
            },
        ],
    }
    "#);
}

#[test]
fn test_parse_meta_name_value() {
    let input = "foo = 5";
    let (inner, meta) = (input, input);

    snapshot!(inner as MetaNameValue, @r#"
    MetaNameValue {
        path: Path {
            segments: [
                PathSegment {
                    ident: "foo",
                },
            ],
        },
        value: Expr::Lit {
            lit: 5,
        },
    }
    "#);

    snapshot!(meta as Meta, @r#"
    Meta::NameValue {
        path: Path {
            segments: [
                PathSegment {
                    ident: "foo",
                },
            ],
        },
        value: Expr::Lit {
            lit: 5,
        },
    }
    "#);

    assert_eq!(meta, Meta::NameValue(inner));
}

#[test]
fn test_parse_meta_item_list_lit() {
    let input = "foo(5)";
    let (inner, meta) = (input, input);

    snapshot!(inner as MetaList, @r#"
    MetaList {
        path: Path {
            segments: [
                PathSegment {
                    ident: "foo",
                },
            ],
        },
        delimiter: MacroDelimiter::Paren,
        tokens: TokenStream(`5`),
    }
    "#);

    snapshot!(meta as Meta, @r#"
    Meta::List {
        path: Path {
            segments: [
                PathSegment {
                    ident: "foo",
                },
            ],
        },
        delimiter: MacroDelimiter::Paren,
        tokens: TokenStream(`5`),
    }
    "#);

    assert_eq!(meta, Meta::List(inner));
}

#[test]
fn test_parse_meta_item_multiple() {
    let input = "foo(word, name = 5, list(name2 = 6), word2)";
    let (inner, meta) = (input, input);

    snapshot!(inner as MetaList, @r#"
    MetaList {
        path: Path {
            segments: [
                PathSegment {
                    ident: "foo",
                },
            ],
        },
        delimiter: MacroDelimiter::Paren,
        tokens: TokenStream(`word , name = 5 , list (name2 = 6) , word2`),
    }
    "#);

    snapshot!(meta as Meta, @r#"
    Meta::List {
        path: Path {
            segments: [
                PathSegment {
                    ident: "foo",
                },
            ],
        },
        delimiter: MacroDelimiter::Paren,
        tokens: TokenStream(`word , name = 5 , list (name2 = 6) , word2`),
    }
    "#);

    assert_eq!(meta, Meta::List(inner));
}

#[test]
fn test_parse_path() {
    let input = "::serde::Serialize";
    snapshot!(input as Meta, @r#"
    Meta::Path {
        leading_colon: Some,
        segments: [
            PathSegment {
                ident: "serde",
            },
            Token![::],
            PathSegment {
                ident: "Serialize",
            },
        ],
    }
    "#);
}

#[test]
fn test_fat_arrow_after_meta() {
    fn parse(input: ParseStream) -> Result<()> {
        while !input.is_empty() {
            let _: Meta = input.parse()?;
            let _: Token![=>] = input.parse()?;
            let brace;
            syn::braced!(brace in input);
        }
        Ok(())
    }

    let input = quote! {
        target_os = "linux" => {}
        windows => {}
    };

    parse.parse2(input).unwrap();
}
