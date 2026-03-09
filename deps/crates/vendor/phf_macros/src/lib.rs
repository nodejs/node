//! A set of macros to generate Rust source for PHF data structures at compile time.
//! See [the `phf` crate's documentation][phf] for details.
//!
//! [phf]: https://docs.rs/phf

use phf_generator::HashState;
use phf_shared::PhfHash;
use proc_macro::TokenStream;
use quote::quote;
use std::collections::HashSet;
use std::hash::Hasher;
use syn::parse::{self, Parse, ParseStream};
use syn::punctuated::Punctuated;
use syn::{parse_macro_input, Error, Expr, ExprLit, Lit, Token, UnOp};
#[cfg(feature = "unicase")]
use unicase_::UniCase;

#[derive(Hash, PartialEq, Eq, Clone)]
enum ParsedKey {
    Str(String),
    Binary(Vec<u8>),
    Char(char),
    I8(i8),
    I16(i16),
    I32(i32),
    I64(i64),
    I128(i128),
    Isize(isize),
    U8(u8),
    U16(u16),
    U32(u32),
    U64(u64),
    U128(u128),
    Usize(usize),
    Bool(bool),
    #[cfg(feature = "unicase")]
    UniCase(UniCase<String>),
}

impl PhfHash for ParsedKey {
    fn phf_hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        match self {
            ParsedKey::Str(s) => s.phf_hash(state),
            ParsedKey::Binary(s) => s.phf_hash(state),
            ParsedKey::Char(s) => s.phf_hash(state),
            ParsedKey::I8(s) => s.phf_hash(state),
            ParsedKey::I16(s) => s.phf_hash(state),
            ParsedKey::I32(s) => s.phf_hash(state),
            ParsedKey::I64(s) => s.phf_hash(state),
            ParsedKey::I128(s) => s.phf_hash(state),
            ParsedKey::Isize(s) => s.phf_hash(state),
            ParsedKey::U8(s) => s.phf_hash(state),
            ParsedKey::U16(s) => s.phf_hash(state),
            ParsedKey::U32(s) => s.phf_hash(state),
            ParsedKey::U64(s) => s.phf_hash(state),
            ParsedKey::U128(s) => s.phf_hash(state),
            ParsedKey::Usize(s) => s.phf_hash(state),
            ParsedKey::Bool(s) => s.phf_hash(state),
            #[cfg(feature = "unicase")]
            ParsedKey::UniCase(s) => s.phf_hash(state),
        }
    }
}

impl ParsedKey {
    fn from_expr(expr: &Expr) -> Option<ParsedKey> {
        match expr {
            Expr::Lit(lit) => match &lit.lit {
                Lit::Str(s) => Some(ParsedKey::Str(s.value())),
                Lit::ByteStr(s) => Some(ParsedKey::Binary(s.value())),
                Lit::Byte(s) => Some(ParsedKey::U8(s.value())),
                Lit::Char(s) => Some(ParsedKey::Char(s.value())),
                Lit::Int(s) => match s.suffix() {
                    // we've lost the sign at this point, so `-128i8` looks like `128i8`,
                    // which doesn't fit in an `i8`; parse it as a `u8` and cast (to `0i8`),
                    // which is handled below, by `Unary`
                    "i8" => Some(ParsedKey::I8(s.base10_parse::<u8>().unwrap() as i8)),
                    "i16" => Some(ParsedKey::I16(s.base10_parse::<u16>().unwrap() as i16)),
                    "i32" => Some(ParsedKey::I32(s.base10_parse::<u32>().unwrap() as i32)),
                    "i64" => Some(ParsedKey::I64(s.base10_parse::<u64>().unwrap() as i64)),
                    "i128" => Some(ParsedKey::I128(s.base10_parse::<u128>().unwrap() as i128)),
                    "isize" => Some(ParsedKey::Isize(s.base10_parse::<usize>().unwrap() as isize)),
                    "u8" => Some(ParsedKey::U8(s.base10_parse::<u8>().unwrap())),
                    "u16" => Some(ParsedKey::U16(s.base10_parse::<u16>().unwrap())),
                    "u32" => Some(ParsedKey::U32(s.base10_parse::<u32>().unwrap())),
                    "u64" => Some(ParsedKey::U64(s.base10_parse::<u64>().unwrap())),
                    "u128" => Some(ParsedKey::U128(s.base10_parse::<u128>().unwrap())),
                    "usize" => Some(ParsedKey::Usize(s.base10_parse::<usize>().unwrap())),
                    _ => None,
                },
                Lit::Bool(s) => Some(ParsedKey::Bool(s.value)),
                _ => None,
            },
            Expr::Array(array) => {
                let mut buf = vec![];
                for expr in &array.elems {
                    match expr {
                        Expr::Lit(lit) => match &lit.lit {
                            Lit::Int(s) => match s.suffix() {
                                "u8" | "" => buf.push(s.base10_parse::<u8>().unwrap()),
                                _ => return None,
                            },
                            _ => return None,
                        },
                        _ => return None,
                    }
                }
                Some(ParsedKey::Binary(buf))
            }
            Expr::Unary(unary) => {
                // if we received an integer literal (always unsigned) greater than i__::max_value()
                // then casting it to a signed integer type of the same width will negate it to
                // the same absolute value so we don't need to negate it here
                macro_rules! try_negate (
                    ($val:expr) => {if $val < 0 { $val } else { -$val }}
                );

                match unary.op {
                    UnOp::Neg(_) => match ParsedKey::from_expr(&unary.expr)? {
                        ParsedKey::I8(v) => Some(ParsedKey::I8(try_negate!(v))),
                        ParsedKey::I16(v) => Some(ParsedKey::I16(try_negate!(v))),
                        ParsedKey::I32(v) => Some(ParsedKey::I32(try_negate!(v))),
                        ParsedKey::I64(v) => Some(ParsedKey::I64(try_negate!(v))),
                        ParsedKey::I128(v) => Some(ParsedKey::I128(try_negate!(v))),
                        ParsedKey::Isize(v) => Some(ParsedKey::Isize(try_negate!(v))),
                        _ => None,
                    },
                    UnOp::Deref(_) => {
                        let mut expr = &*unary.expr;
                        while let Expr::Group(group) = expr {
                            expr = &*group.expr;
                        }
                        match expr {
                            Expr::Lit(ExprLit {
                                lit: Lit::ByteStr(s),
                                ..
                            }) => Some(ParsedKey::Binary(s.value())),
                            _ => None,
                        }
                    }
                    _ => None,
                }
            }
            Expr::Group(group) => ParsedKey::from_expr(&group.expr),
            #[cfg(feature = "unicase")]
            Expr::Call(call) => {
                if let Expr::Path(ep) = call.func.as_ref() {
                    let segments = &mut ep.path.segments.iter().rev();
                    let last = &segments.next()?.ident;
                    let last_ahead = &segments.next()?.ident;
                    let is_unicode = last_ahead == "UniCase" && last == "unicode";
                    let is_ascii = last_ahead == "UniCase" && last == "ascii";
                    if call.args.len() == 1 && (is_unicode || is_ascii) {
                        if let Some(Expr::Lit(ExprLit {
                            attrs: _,
                            lit: Lit::Str(s),
                        })) = call.args.first()
                        {
                            let v = if is_unicode {
                                UniCase::unicode(s.value())
                            } else {
                                UniCase::ascii(s.value())
                            };
                            Some(ParsedKey::UniCase(v))
                        } else {
                            None
                        }
                    } else {
                        None
                    }
                } else {
                    None
                }
            }
            _ => None,
        }
    }
}

struct Key {
    parsed: ParsedKey,
    expr: Expr,
}

impl PhfHash for Key {
    fn phf_hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        self.parsed.phf_hash(state)
    }
}

impl Parse for Key {
    fn parse(input: ParseStream<'_>) -> parse::Result<Key> {
        let expr = input.parse()?;
        let parsed = ParsedKey::from_expr(&expr)
            .ok_or_else(|| Error::new_spanned(&expr, "unsupported key expression"))?;

        Ok(Key { parsed, expr })
    }
}

struct Entry {
    key: Key,
    value: Expr,
}

impl PhfHash for Entry {
    fn phf_hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        self.key.phf_hash(state)
    }
}

impl Parse for Entry {
    fn parse(input: ParseStream<'_>) -> parse::Result<Entry> {
        let key = input.parse()?;
        input.parse::<Token![=>]>()?;
        let value = input.parse()?;
        Ok(Entry { key, value })
    }
}

struct Map(Vec<Entry>);

impl Parse for Map {
    fn parse(input: ParseStream<'_>) -> parse::Result<Map> {
        let parsed = Punctuated::<Entry, Token![,]>::parse_terminated(input)?;
        let map = parsed.into_iter().collect::<Vec<_>>();
        check_duplicates(&map)?;
        Ok(Map(map))
    }
}

struct Set(Vec<Entry>);

impl Parse for Set {
    fn parse(input: ParseStream<'_>) -> parse::Result<Set> {
        let parsed = Punctuated::<Key, Token![,]>::parse_terminated(input)?;
        let set = parsed
            .into_iter()
            .map(|key| Entry {
                key,
                value: syn::parse_str("()").unwrap(),
            })
            .collect::<Vec<_>>();
        check_duplicates(&set)?;
        Ok(Set(set))
    }
}

fn check_duplicates(entries: &[Entry]) -> parse::Result<()> {
    let mut keys = HashSet::new();
    for entry in entries {
        if !keys.insert(&entry.key.parsed) {
            return Err(Error::new_spanned(&entry.key.expr, "duplicate key"));
        }
    }
    Ok(())
}

fn build_map(entries: &[Entry], state: HashState) -> proc_macro2::TokenStream {
    let key = state.key;
    let disps = state.disps.iter().map(|&(d1, d2)| quote!((#d1, #d2)));
    let entries = state.map.iter().map(|&idx| {
        let key = &entries[idx].key.expr;
        let value = &entries[idx].value;
        quote!((#key, #value))
    });

    quote! {
        phf::Map {
            key: #key,
            disps: &[#(#disps),*],
            entries: &[#(#entries),*],
        }
    }
}

fn build_ordered_map(entries: &[Entry], state: HashState) -> proc_macro2::TokenStream {
    let key = state.key;
    let disps = state.disps.iter().map(|&(d1, d2)| quote!((#d1, #d2)));
    let idxs = state.map.iter().map(|idx| quote!(#idx));
    let entries = entries.iter().map(|entry| {
        let key = &entry.key.expr;
        let value = &entry.value;
        quote!((#key, #value))
    });

    quote! {
        phf::OrderedMap {
            key: #key,
            disps: &[#(#disps),*],
            idxs: &[#(#idxs),*],
            entries: &[#(#entries),*],
        }
    }
}

#[proc_macro]
pub fn phf_map(input: TokenStream) -> TokenStream {
    let map = parse_macro_input!(input as Map);
    let state = phf_generator::generate_hash(&map.0);

    build_map(&map.0, state).into()
}

#[proc_macro]
pub fn phf_set(input: TokenStream) -> TokenStream {
    let set = parse_macro_input!(input as Set);
    let state = phf_generator::generate_hash(&set.0);

    let map = build_map(&set.0, state);
    quote!(phf::Set { map: #map }).into()
}

#[proc_macro]
pub fn phf_ordered_map(input: TokenStream) -> TokenStream {
    let map = parse_macro_input!(input as Map);
    let state = phf_generator::generate_hash(&map.0);

    build_ordered_map(&map.0, state).into()
}

#[proc_macro]
pub fn phf_ordered_set(input: TokenStream) -> TokenStream {
    let set = parse_macro_input!(input as Set);
    let state = phf_generator::generate_hash(&set.0);

    let map = build_ordered_map(&set.0, state);
    quote!(phf::OrderedSet { map: #map }).into()
}
