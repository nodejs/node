//! [![github]](https://github.com/dtolnay/seq-macro)&ensp;[![crates-io]](https://crates.io/crates/seq-macro)&ensp;[![docs-rs]](https://docs.rs/seq-macro)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs
//!
//! <br>
//!
//! # Imagine for-loops in a macro
//!
//! This crate provides a `seq!` macro to repeat a fragment of source code and
//! substitute into each repetition a sequential numeric counter.
//!
//! ```
//! use seq_macro::seq;
//!
//! fn main() {
//!     let tuple = (1000, 100, 10);
//!     let mut sum = 0;
//!
//!     // Expands to:
//!     //
//!     //     sum += tuple.0;
//!     //     sum += tuple.1;
//!     //     sum += tuple.2;
//!     //
//!     // This cannot be written using an ordinary for-loop because elements of
//!     // a tuple can only be accessed by their integer literal index, not by a
//!     // variable.
//!     seq!(N in 0..=2 {
//!         sum += tuple.N;
//!     });
//!
//!     assert_eq!(sum, 1110);
//! }
//! ```
//!
//! - If the input tokens contain a section surrounded by `#(` ... `)*` then
//!   only that part is repeated.
//!
//! - The numeric counter can be pasted onto the end of some prefix to form
//!   sequential identifiers.
//!
//! ```
//! use seq_macro::seq;
//!
//! seq!(N in 64..=127 {
//!     #[derive(Debug)]
//!     enum Demo {
//!         // Expands to Variant64, Variant65, ...
//!         ##(
//!             Variant~N,
//!         )*
//!     }
//! });
//!
//! fn main() {
//!     assert_eq!("Variant99", format!("{:?}", Demo::Variant99));
//! }
//! ```
//!
//! - Byte and character ranges are supported: `b'a'..=b'z'`, `'a'..='z'`.
//!
//! - If the range bounds are written in binary, octal, hex, or with zero
//!   padding, those features are preserved in any generated tokens.
//!
//! ```
//! use seq_macro::seq;
//!
//! seq!(P in 0x000..=0x00F {
//!     // expands to structs Pin000, ..., Pin009, Pin00A, ..., Pin00F
//!     struct Pin~P;
//! });
//! ```

#![doc(html_root_url = "https://docs.rs/seq-macro/0.3.6")]
#![allow(
    clippy::cast_lossless,
    clippy::cast_possible_truncation,
    clippy::derive_partial_eq_without_eq,
    clippy::into_iter_without_iter,
    clippy::let_underscore_untyped,
    clippy::needless_doctest_main,
    clippy::single_match_else,
    clippy::wildcard_imports
)]

mod parse;

use crate::parse::*;
use proc_macro::{Delimiter, Group, Ident, Literal, Span, TokenStream, TokenTree};
use std::char;
use std::iter::{self, FromIterator};

#[proc_macro]
pub fn seq(input: TokenStream) -> TokenStream {
    match seq_impl(input) {
        Ok(expanded) => expanded,
        Err(error) => error.into_compile_error(),
    }
}

struct Range {
    begin: u64,
    end: u64,
    inclusive: bool,
    kind: Kind,
    suffix: String,
    width: usize,
    radix: Radix,
}

struct Value {
    int: u64,
    kind: Kind,
    suffix: String,
    width: usize,
    radix: Radix,
    span: Span,
}

struct Splice<'a> {
    int: u64,
    kind: Kind,
    suffix: &'a str,
    width: usize,
    radix: Radix,
}

#[derive(Copy, Clone, PartialEq)]
enum Kind {
    Int,
    Byte,
    Char,
}

#[derive(Copy, Clone, PartialEq)]
enum Radix {
    Binary,
    Octal,
    Decimal,
    LowerHex,
    UpperHex,
}

impl<'a> IntoIterator for &'a Range {
    type Item = Splice<'a>;
    type IntoIter = Box<dyn Iterator<Item = Splice<'a>> + 'a>;

    fn into_iter(self) -> Self::IntoIter {
        let splice = move |int| Splice {
            int,
            kind: self.kind,
            suffix: &self.suffix,
            width: self.width,
            radix: self.radix,
        };
        match self.kind {
            Kind::Int | Kind::Byte => {
                if self.inclusive {
                    Box::new((self.begin..=self.end).map(splice))
                } else {
                    Box::new((self.begin..self.end).map(splice))
                }
            }
            Kind::Char => {
                let begin = char::from_u32(self.begin as u32).unwrap();
                let end = char::from_u32(self.end as u32).unwrap();
                let int = |ch| u64::from(u32::from(ch));
                if self.inclusive {
                    Box::new((begin..=end).map(int).map(splice))
                } else {
                    Box::new((begin..end).map(int).map(splice))
                }
            }
        }
    }
}

fn seq_impl(input: TokenStream) -> Result<TokenStream, SyntaxError> {
    let mut iter = input.into_iter();
    let var = require_ident(&mut iter)?;
    require_keyword(&mut iter, "in")?;
    let begin = require_value(&mut iter)?;
    require_punct(&mut iter, '.')?;
    require_punct(&mut iter, '.')?;
    let inclusive = require_if_punct(&mut iter, '=')?;
    let end = require_value(&mut iter)?;
    let body = require_braces(&mut iter)?;
    require_end(&mut iter)?;

    let range = validate_range(begin, end, inclusive)?;

    let mut found_repetition = false;
    let expanded = expand_repetitions(&var, &range, body.clone(), &mut found_repetition);
    if found_repetition {
        Ok(expanded)
    } else {
        // If no `#(...)*`, repeat the entire body.
        Ok(repeat(&var, &range, &body))
    }
}

fn repeat(var: &Ident, range: &Range, body: &TokenStream) -> TokenStream {
    let mut repeated = TokenStream::new();
    for value in range {
        repeated.extend(substitute_value(var, &value, body.clone()));
    }
    repeated
}

fn substitute_value(var: &Ident, splice: &Splice, body: TokenStream) -> TokenStream {
    let mut tokens = Vec::from_iter(body);

    let mut i = 0;
    while i < tokens.len() {
        // Substitute our variable by itself, e.g. `N`.
        let replace = match &tokens[i] {
            TokenTree::Ident(ident) => ident.to_string() == var.to_string(),
            _ => false,
        };
        if replace {
            let original_span = tokens[i].span();
            let mut literal = splice.literal();
            literal.set_span(original_span);
            tokens[i] = TokenTree::Literal(literal);
            i += 1;
            continue;
        }

        // Substitute our variable concatenated onto some prefix, `Prefix~N`.
        if i + 3 <= tokens.len() {
            let prefix = match &tokens[i..i + 3] {
                [first, TokenTree::Punct(tilde), TokenTree::Ident(ident)]
                    if tilde.as_char() == '~' && ident.to_string() == var.to_string() =>
                {
                    match first {
                        TokenTree::Ident(ident) => Some(ident.clone()),
                        TokenTree::Group(group) => {
                            let mut iter = group.stream().into_iter().fuse();
                            match (iter.next(), iter.next()) {
                                (Some(TokenTree::Ident(ident)), None) => Some(ident),
                                _ => None,
                            }
                        }
                        _ => None,
                    }
                }
                _ => None,
            };
            if let Some(prefix) = prefix {
                let number = match splice.kind {
                    Kind::Int => match splice.radix {
                        Radix::Binary => format!("{0:01$b}", splice.int, splice.width),
                        Radix::Octal => format!("{0:01$o}", splice.int, splice.width),
                        Radix::Decimal => format!("{0:01$}", splice.int, splice.width),
                        Radix::LowerHex => format!("{0:01$x}", splice.int, splice.width),
                        Radix::UpperHex => format!("{0:01$X}", splice.int, splice.width),
                    },
                    Kind::Byte | Kind::Char => {
                        char::from_u32(splice.int as u32).unwrap().to_string()
                    }
                };
                let concat = format!("{}{}", prefix, number);
                let ident = Ident::new(&concat, prefix.span());
                tokens.splice(i..i + 3, iter::once(TokenTree::Ident(ident)));
                i += 1;
                continue;
            }
        }

        // Recursively substitute content nested in a group.
        if let TokenTree::Group(group) = &mut tokens[i] {
            let original_span = group.span();
            let content = substitute_value(var, splice, group.stream());
            *group = Group::new(group.delimiter(), content);
            group.set_span(original_span);
        }

        i += 1;
    }

    TokenStream::from_iter(tokens)
}

fn enter_repetition(tokens: &[TokenTree]) -> Option<TokenStream> {
    assert!(tokens.len() == 3);
    match &tokens[0] {
        TokenTree::Punct(punct) if punct.as_char() == '#' => {}
        _ => return None,
    }
    match &tokens[2] {
        TokenTree::Punct(punct) if punct.as_char() == '*' => {}
        _ => return None,
    }
    match &tokens[1] {
        TokenTree::Group(group) if group.delimiter() == Delimiter::Parenthesis => {
            Some(group.stream())
        }
        _ => None,
    }
}

fn expand_repetitions(
    var: &Ident,
    range: &Range,
    body: TokenStream,
    found_repetition: &mut bool,
) -> TokenStream {
    let mut tokens = Vec::from_iter(body);

    // Look for `#(...)*`.
    let mut i = 0;
    while i < tokens.len() {
        if let TokenTree::Group(group) = &mut tokens[i] {
            let content = expand_repetitions(var, range, group.stream(), found_repetition);
            let original_span = group.span();
            *group = Group::new(group.delimiter(), content);
            group.set_span(original_span);
            i += 1;
            continue;
        }
        if i + 3 > tokens.len() {
            i += 1;
            continue;
        }
        let template = match enter_repetition(&tokens[i..i + 3]) {
            Some(template) => template,
            None => {
                i += 1;
                continue;
            }
        };
        *found_repetition = true;
        let mut repeated = Vec::new();
        for value in range {
            repeated.extend(substitute_value(var, &value, template.clone()));
        }
        let repeated_len = repeated.len();
        tokens.splice(i..i + 3, repeated);
        i += repeated_len;
    }

    TokenStream::from_iter(tokens)
}

impl Splice<'_> {
    fn literal(&self) -> Literal {
        match self.kind {
            Kind::Int | Kind::Byte => {
                let repr = match self.radix {
                    Radix::Binary => format!("0b{0:02$b}{1}", self.int, self.suffix, self.width),
                    Radix::Octal => format!("0o{0:02$o}{1}", self.int, self.suffix, self.width),
                    Radix::Decimal => format!("{0:02$}{1}", self.int, self.suffix, self.width),
                    Radix::LowerHex => format!("0x{0:02$x}{1}", self.int, self.suffix, self.width),
                    Radix::UpperHex => format!("0x{0:02$X}{1}", self.int, self.suffix, self.width),
                };
                let tokens = repr.parse::<TokenStream>().unwrap();
                let mut iter = tokens.into_iter();
                let literal = match iter.next() {
                    Some(TokenTree::Literal(literal)) => literal,
                    _ => unreachable!(),
                };
                assert!(iter.next().is_none());
                literal
            }
            Kind::Char => {
                let ch = char::from_u32(self.int as u32).unwrap();
                Literal::character(ch)
            }
        }
    }
}
