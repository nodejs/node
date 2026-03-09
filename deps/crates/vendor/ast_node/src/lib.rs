#![deny(clippy::all)]
#![recursion_limit = "1024"]

extern crate proc_macro;

use quote::quote;
use swc_macros_common::prelude::*;
use syn::{visit_mut::VisitMut, *};

mod ast_node_macro;
mod encoding;
mod enum_deserialize;
mod spanned;

/// Derives [`swc_common::Spanned`]. See [`swc_common::Spanned`] for
/// documentation.
#[proc_macro_derive(Spanned, attributes(span))]
pub fn derive_spanned(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = parse::<DeriveInput>(input).expect("failed to parse input as DeriveInput");

    let item = self::spanned::derive(input);

    print("derive(Spanned)", item.into_token_stream())
}

/// Derives `serde::Deserialize` which is aware of `tag` based deserialization.
#[proc_macro_derive(DeserializeEnum, attributes(tag, encoding))]
pub fn derive_deserialize_enum(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = parse::<DeriveInput>(input).expect("failed to parse input as DeriveInput");

    let item = enum_deserialize::expand(input);

    print("derive(DeserializeEnum)", item.into_token_stream())
}

#[proc_macro_derive(Encode, attributes(encoding))]
pub fn derive_encode(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input =
        syn::parse::<syn::DeriveInput>(input).expect("failed to parse input as DeriveInput");

    let item = encoding::encode::expand(input);
    print("derive(Encode)", item.into_token_stream())
}

#[proc_macro_derive(Decode, attributes(encoding))]
pub fn derive_decode(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input =
        syn::parse::<syn::DeriveInput>(input).expect("failed to parse input as DeriveInput");

    let item = encoding::decode::expand(input);
    print("derive(Decode)", item.into_token_stream())
}

/// Derives `serde::Serialize` and `serde::Deserialize`.
///
/// # Struct attributes
///
/// `#[ast_serde("A")]` adds `"type": "A"` to json when serialized, and
/// deserializes as the type only if `type` field of json string is `A`.
///
/// # Enum attributes
///
/// ## Type-level attributes
///
/// This macro does not accept arguments if used on enum.
///
/// ## Variant attributes
///
/// ### `#[tag("Expr")]`
///
/// You can tell "Use this variant if `type` is `Expr`".
///
/// This attribute can be applied multiple time, if a variant consumes multiple
/// `type`s.
///
/// For example, `Lit` of swc_ecma_ast is an enum, but `Expr`, which contains
/// `Lit` as a variant, is also an enum.
/// So the `Lit` variant has multiple `#[tag]`-s like
///
/// ```rust,ignore
/// enum Expr {
///   #[tag("StringLiteral")]
///   #[tag("NumericLiteral")]
///   #[tag("BooleanLiteral")]
///   Lit(Lit),
/// }
/// ```
///
/// so the deserializer can decide which variant to use.
///
///
/// `#[tag]` also supports wildcard like `#[tag("*")]`. You can use this if
/// there are two many variants.
#[proc_macro_attribute]
pub fn ast_serde(
    args: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let input: DeriveInput = parse(input).expect("failed to parse input as a DeriveInput");

    // we should use call_site
    let mut item = TokenStream::new();
    match input.data {
        Data::Enum(..) => {
            if !args.is_empty() {
                panic!("#[ast_serde] on enum does not accept any argument")
            }

            item.extend(quote!(
                #[derive(::serde::Serialize, ::swc_common::DeserializeEnum)]
                #[serde(untagged)]
                #input
            ));
        }
        _ => {
            let args: Option<ast_node_macro::Args> = if args.is_empty() {
                None
            } else {
                Some(parse(args).expect("failed to parse args of #[ast_serde]"))
            };

            let serde_tag = match input.data {
                Data::Struct(DataStruct {
                    fields: Fields::Named(..),
                    ..
                }) => {
                    if args.is_some() {
                        Some(quote!(#[serde(tag = "type")]))
                    } else {
                        None
                    }
                }
                _ => None,
            };

            let serde_rename = args.as_ref().map(|args| {
                let name = &args.ty;
                quote!(#[serde(rename = #name)])
            });

            item.extend(quote!(
                #[derive(::serde::Serialize, ::serde::Deserialize)]
                #serde_tag
                #[serde(rename_all = "camelCase")]
                #serde_rename
                #input
            ));
        }
    };

    print("ast_serde", item)
}

struct AddAttr;

impl VisitMut for AddAttr {
    fn visit_field_mut(&mut self, f: &mut Field) {
        f.attrs
            .push(parse_quote!(#[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]));
    }
}

/// Alias for
/// `#[derive(Spanned, Fold, Clone, Debug, PartialEq)]` for a struct and
/// `#[derive(Spanned, Fold, Clone, Debug, PartialEq, FromVariant)]` for an
/// enum.
#[proc_macro_attribute]
pub fn ast_node(
    args: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let mut input: DeriveInput = parse(input).expect("failed to parse input as a DeriveInput");

    AddAttr.visit_data_mut(&mut input.data);

    // we should use call_site
    let mut item = TokenStream::new();
    match &input.data {
        Data::Enum(data) => {
            use syn::parse::Parser;

            let attrs = <syn::punctuated::Punctuated<syn::Ident, syn::Token![,]>>::parse_terminated
                .parse(args)
                .expect("failed to parse #[ast_node]");

            let mut has_no_clone = false;
            let mut has_no_unknown = false;
            for attr in &attrs {
                if attr == "no_clone" {
                    has_no_clone = true;
                } else if attr == "no_unknown" {
                    has_no_unknown = true;
                } else {
                    panic!("unknown attribute: {attr:?}")
                }
            }

            let clone = if !has_no_clone {
                Some(quote!(#[derive(Clone)]))
            } else {
                None
            };
            let non_exhaustive = if !has_no_unknown {
                Some(quote!(#[cfg_attr(swc_ast_unknown, non_exhaustive)]))
            } else {
                None
            };

            let mut data = data.clone();
            if !has_no_unknown {
                let unknown: syn::Variant = if data
                    .variants
                    .iter()
                    .all(|variant| variant.fields.is_empty())
                {
                    syn::parse_quote! {
                        #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
                        #[from_variant(ignore)]
                        #[span(unknown)]
                        #[encoding(unknown)]
                        Unknown(u32)
                    }
                } else {
                    syn::parse_quote! {
                        #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
                        #[from_variant(ignore)]
                        #[span(unknown)]
                        #[encoding(unknown)]
                        Unknown(u32, swc_common::unknown::Unknown)
                    }
                };

                // insert unknown member
                data.variants.insert(0, unknown);
                input.data = Data::Enum(data);
            }

            item.extend(quote!(
                #[allow(clippy::derive_partial_eq_without_eq)]
                #[cfg_attr(
                    feature = "serde-impl",
                    derive(
                        ::serde::Serialize,
                    )
                )]
                #[derive(
                    ::swc_common::FromVariant,
                    ::swc_common::Spanned,
                    Debug,
                    PartialEq,
                    ::swc_common::DeserializeEnum,
                )]
                #clone
                #non_exhaustive
                #[cfg_attr(
                    feature = "rkyv-impl",
                    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
                )]
                #[cfg_attr(
                    feature = "rkyv-impl",
                    rkyv(deserialize_bounds(__D::Error: rkyv::rancor::Source))
                )]
                #[cfg_attr(feature = "rkyv-impl", repr(u32))]
                #[cfg_attr(
                    feature = "rkyv-impl",
                    rkyv(serialize_bounds(__S: rkyv::ser::Writer + rkyv::ser::Allocator,
                        __S::Error: rkyv::rancor::Source))
                )]
                #[cfg_attr(
                    feature = "rkyv-impl",
                    rkyv(bytecheck(bounds(
                        __C: rkyv::validation::ArchiveContext,
                        __C::Error: rkyv::rancor::Source
                    )))
                )]
                #[cfg_attr(
                    feature = "serde-impl",
                    serde(untagged)
                )]
                #[cfg_attr(
                    feature = "encoding-impl",
                    derive(::swc_common::Encode, ::swc_common::Decode)
                )]
                #input
            ));
        }
        _ => {
            let args: Option<ast_node_macro::Args> = if args.is_empty() {
                None
            } else {
                Some(parse(args).expect("failed to parse args of #[ast_node]"))
            };

            let serde_tag = match input.data {
                Data::Struct(DataStruct {
                    fields: Fields::Named(..),
                    ..
                }) => {
                    if args.is_some() {
                        Some(quote!(#[cfg_attr(
                            feature = "serde-impl",
                            serde(tag = "type")
                        )]))
                    } else {
                        None
                    }
                }
                _ => None,
            };

            let serde_rename = args.as_ref().map(|args| {
                let name = &args.ty;

                quote!(#[cfg_attr(
                    feature = "serde-impl",
                    serde(rename = #name)
                )])
            });

            let ast_node_impl = args
                .as_ref()
                .map(|args| ast_node_macro::expand_struct(args.clone(), input.clone()));

            item.extend(quote!(
                #[allow(clippy::derive_partial_eq_without_eq)]
                #[derive(::swc_common::Spanned, Clone, Debug, PartialEq)]
                #[cfg_attr(
                    feature = "serde-impl",
                    derive(::serde::Serialize, ::serde::Deserialize)
                )]
                #[cfg_attr(
                    feature = "rkyv-impl",
                    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
                )]
                #[cfg_attr(
                    feature = "rkyv-impl",
                    rkyv(deserialize_bounds(__D::Error: rkyv::rancor::Source))
                )]
                #[cfg_attr(
                    feature = "rkyv-impl",
                    rkyv(bytecheck(bounds(
                        __C: rkyv::validation::ArchiveContext,
                        __C::Error: rkyv::rancor::Source
                    )))
                )]
                #[cfg_attr(feature = "rkyv-impl", repr(C))]
                #[cfg_attr(
                    feature = "rkyv-impl",
                    rkyv(serialize_bounds(__S: rkyv::ser::Writer + rkyv::ser::Allocator,
                        __S::Error: rkyv::rancor::Source))
                )]
                #serde_tag
                #[cfg_attr(
                    feature = "serde-impl",
                    serde(rename_all = "camelCase")
                )]
                #serde_rename
                #[cfg_attr(
                    feature = "encoding-impl",
                    derive(::swc_common::Encode, ::swc_common::Decode)
                )]
                #input
            ));

            if let Some(items) = ast_node_impl {
                for item_impl in items {
                    item.extend(item_impl.into_token_stream());
                }
            }
        }
    };

    print("ast_node", item)
}
