#![recursion_limit = "1024"]

extern crate proc_macro;

use quote::quote_spanned;
use swc_macros_common::prelude::*;
use syn::{parse::Parse, *};

/// Creates `.as_str()` and then implements `Debug` and `Display` using it.
///
///# Input
/// Enum with \`str_value\`-style **doc** comment for each variant.
///
/// e.g.
///
///```no_run
/// pub enum BinOp {
///     /// `+`
///     Add,
///     /// `-`
///     Minus,
/// }
/// ```
///
/// Currently, \`str_value\` must be live in it's own line.
///
///# Output
///
///  - `pub fn as_str(&self) -> &'static str`
///  - `impl serde::Serialize` with `cfg(feature = "serde")`
///  - `impl serde::Deserialize` with `cfg(feature = "serde")`
///  - `impl FromStr`
///  - `impl Debug`
///  - `impl Display`
///
///# Example
///
///
///```
/// #[macro_use]
/// extern crate string_enum;
/// extern crate serde;
///
/// #[derive(StringEnum)]
/// pub enum Tokens {
///     /// `a`
///     A,
///     /// `bar`
///     B,
/// }
/// # fn main() {
///
/// assert_eq!(Tokens::A.as_str(), "a");
/// assert_eq!(Tokens::B.as_str(), "bar");
///
/// assert_eq!(Tokens::A.to_string(), "a");
/// assert_eq!(format!("{:?}", Tokens::A), format!("{:?}", "a"));
///
/// # }
/// ```
///
///
/// All formatting flags are handled correctly.
#[proc_macro_derive(StringEnum, attributes(string_enum))]
pub fn derive_string_enum(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = syn::parse::<syn::DeriveInput>(input).expect("failed to parse derive input");
    let mut tts = TokenStream::new();

    make_as_str(&input).to_tokens(&mut tts);
    make_from_str(&input).to_tokens(&mut tts);

    make_serialize(&input).to_tokens(&mut tts);
    make_deserialize(&input).to_tokens(&mut tts);

    derive_fmt(&input, quote_spanned!(Span::call_site() => std::fmt::Debug)).to_tokens(&mut tts);
    derive_fmt(
        &input,
        quote_spanned!(Span::call_site() => std::fmt::Display),
    )
    .to_tokens(&mut tts);

    print("derive(StringEnum)", tts)
}

fn derive_fmt(i: &DeriveInput, trait_path: TokenStream) -> ItemImpl {
    let ty = &i.ident;

    let item: ItemImpl = parse_quote!(
        impl #trait_path for #ty {
            fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
                let s = self.as_str();
                #trait_path::fmt(s, f)
            }
        }
    );

    item.with_generics(i.generics.clone())
}

fn get_str_value(attrs: &[Attribute]) -> String {
    // TODO: Accept multiline string
    let docs: Vec<_> = attrs.iter().filter_map(doc_str).collect();
    for raw_line in docs {
        let line = raw_line.trim();
        if line.starts_with('`') && line.ends_with('`') {
            let mut s: String = line.split_at(1).1.into();
            let new_len = s.len() - 1;
            s.truncate(new_len);
            return s;
        }
    }

    panic!("StringEnum: Cannot determine string value of this variant")
}

fn make_from_str(i: &DeriveInput) -> ItemImpl {
    let arms = Binder::new_from(i)
        .variants()
        .into_iter()
        .map(|v| {
            // Qualified path of variant.
            let qual_name = v.qual_path();

            let str_value = get_str_value(v.attrs());

            let mut pat: Pat = Pat::Lit(ExprLit {
                attrs: Default::default(),
                lit: Lit::Str(LitStr::new(&str_value, Span::call_site())),
            });

            // Handle `string_enum(alias("foo"))`
            for attr in v
                .attrs()
                .iter()
                .filter(|attr| is_attr_name(attr, "string_enum"))
            {
                if let Meta::List(meta) = &attr.meta {
                    let mut cases = Punctuated::default();

                    cases.push(pat);

                    for item in parse2::<FieldAttr>(meta.tokens.clone())
                        .expect("failed to parse `#[string_enum]`")
                        .aliases
                    {
                        cases.push(Pat::Lit(PatLit {
                            attrs: Default::default(),
                            lit: Lit::Str(item.alias),
                        }));
                    }

                    pat = Pat::Or(PatOr {
                        attrs: Default::default(),
                        leading_vert: None,
                        cases,
                    });
                    continue;
                }

                panic!("Unsupported meta: {:#?}", attr.meta);
            }

            let body = match *v.data() {
                Fields::Unit => Box::new(parse_quote!(return Ok(#qual_name))),
                _ => unreachable!("StringEnum requires all variants not to have fields"),
            };

            Arm {
                body,
                attrs: v
                    .attrs()
                    .iter()
                    .filter(|attr| is_attr_name(attr, "cfg"))
                    .cloned()
                    .collect(),
                pat,
                guard: None,
                fat_arrow_token: Default::default(),
                comma: Some(Token![,](def_site())),
            }
        })
        .chain(::std::iter::once(parse_quote!(_ => Err(()))))
        .collect();

    let body = Expr::Match(ExprMatch {
        attrs: Default::default(),
        match_token: Default::default(),
        brace_token: Default::default(),
        expr: Box::new(parse_quote!(s)),
        arms,
    });

    let ty = &i.ident;
    let item: ItemImpl = parse_quote!(
        impl ::std::str::FromStr for #ty {
            type Err = ();

            fn from_str(s: &str) -> Result<Self, ()> {
                #body
            }
        }
    );
    item.with_generics(i.generics.clone())
}

fn make_as_str(i: &DeriveInput) -> ItemImpl {
    let arms = Binder::new_from(i)
        .variants()
        .into_iter()
        .map(|v| {
            // Qualified path of variant.
            let qual_name = v.qual_path();

            let str_value = get_str_value(v.attrs());

            let body = Box::new(parse_quote!(return #str_value));

            let pat = match *v.data() {
                Fields::Unit => Box::new(Pat::Path(PatPath {
                    qself: None,
                    path: qual_name,
                    attrs: Default::default(),
                })),
                _ => Box::new(Pat::Struct(PatStruct {
                    attrs: Default::default(),
                    qself: None,
                    path: qual_name,
                    brace_token: Default::default(),
                    fields: Default::default(),
                    rest: Some(PatRest {
                        attrs: Default::default(),
                        dot2_token: Default::default(),
                    }),
                })),
            };

            Arm {
                body,
                attrs: v
                    .attrs()
                    .iter()
                    .filter(|attr| is_attr_name(attr, "cfg"))
                    .cloned()
                    .collect(),
                pat: Pat::Reference(PatReference {
                    and_token: Default::default(),
                    mutability: None,
                    pat,
                    attrs: Default::default(),
                }),
                guard: None,
                fat_arrow_token: Default::default(),
                comma: Some(Token![,](def_site())),
            }
        })
        .collect();

    let body = Expr::Match(ExprMatch {
        attrs: Default::default(),
        match_token: Default::default(),
        brace_token: Default::default(),
        expr: Box::new(parse_quote!(self)),
        arms,
    });

    let ty = &i.ident;
    let as_str = make_as_str_ident();
    let item: ItemImpl = parse_quote!(
        impl #ty {
            pub fn #as_str(&self) -> &'static str {
                #body
            }
        }
    );

    item.with_generics(i.generics.clone())
}

fn make_as_str_ident() -> Ident {
    Ident::new("as_str", call_site())
}

fn make_serialize(i: &DeriveInput) -> ItemImpl {
    let ty = &i.ident;
    let item: ItemImpl = parse_quote!(
        #[cfg(feature = "serde")]
        impl ::serde::Serialize for #ty {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: ::serde::Serializer,
            {
                serializer.serialize_str(self.as_str())
            }
        }
    );

    item.with_generics(i.generics.clone())
}

fn make_deserialize(i: &DeriveInput) -> ItemImpl {
    let ty = &i.ident;
    let item: ItemImpl = parse_quote!(
        #[cfg(feature = "serde")]
        impl<'de> ::serde::Deserialize<'de> for #ty {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: ::serde::Deserializer<'de>,
            {
                struct StrVisitor;

                impl<'de> ::serde::de::Visitor<'de> for StrVisitor {
                    type Value = #ty;

                    fn expecting(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                        // TODO: List strings
                        write!(f, "one of (TODO)")
                    }

                    fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
                    where
                        E: ::serde::de::Error,
                    {
                        // TODO
                        value.parse().map_err(|()| E::unknown_variant(value, &[]))
                    }
                }

                deserializer.deserialize_str(StrVisitor)
            }
        }
    );

    item.with_generics(i.generics.clone())
}

struct FieldAttr {
    aliases: Punctuated<FieldAttrItem, Token![,]>,
}

impl Parse for FieldAttr {
    fn parse(input: parse::ParseStream) -> Result<Self> {
        Ok(Self {
            aliases: input.call(Punctuated::parse_terminated)?,
        })
    }
}

/// `alias("text")` in `#[string_enum(alias("text"))]`.
struct FieldAttrItem {
    alias: LitStr,
}

impl Parse for FieldAttrItem {
    fn parse(input: parse::ParseStream) -> Result<Self> {
        let name: Ident = input.parse()?;

        assert!(
            name == "alias",
            "#[derive(StringEnum) only supports `#[string_enum(alias(\"text\"))]]"
        );

        let alias;
        parenthesized!(alias in input);

        Ok(Self {
            alias: alias.parse()?,
        })
    }
}
