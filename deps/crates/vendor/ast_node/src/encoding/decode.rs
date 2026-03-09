use syn::{spanned::Spanned, Data, DeriveInput};

use super::{is_ignore, is_unknown, is_with, EnumType};

pub fn expand(DeriveInput { ident, data, .. }: DeriveInput) -> syn::ItemImpl {
    match data {
        Data::Struct(data) => {
            let is_named = data.fields.iter().any(|field| field.ident.is_some());
            let names = data
                .fields
                .iter()
                .enumerate()
                .map(|(idx, field)| match field.ident.as_ref() {
                    Some(name) => name.clone(),
                    None => {
                        let name = format!("unit{idx}");
                        syn::Ident::new(&name, field.span())
                    }
                })
                .collect::<Vec<_>>();

            let fields = data.fields.iter()
                .zip(names.iter())
                .map(|(field, field_name)| -> syn::Stmt {
                    let ty = &field.ty;
                    let value: syn::Expr = match (is_with(&field.attrs), is_ignore(&field.attrs)) {
                        (Some(with_type), false) => syn::parse_quote!(<#with_type<#ty> as cbor4ii::core::dec::Decode<'_>>::decode(reader)?.0),
                        (None, false) => syn::parse_quote!(<#ty as cbor4ii::core::dec::Decode<'_>>::decode(reader)?),
                        (None, true) => syn::parse_quote!(<#ty as Default>::default()),
                        (Some(_), true) => panic!("Cannot use both #[encoding(with)] and #[encoding(ignore)] attributes on the same field")
                    };

                    syn::parse_quote!{
                        let #field_name = #value;
                    }
                });
            let build_struct: syn::Expr = if is_named {
                syn::parse_quote! { #ident { #(#names),* } }
            } else {
                syn::parse_quote! { #ident ( #(#names),* ) }
            };

            let count = data.fields.len();
            let head: Option<syn::Stmt> = (count != 1).then(|| {
                syn::parse_quote! {
                    let len = <cbor4ii::core::types::Array<()>>::len(reader)?.unwrap();
                }
            });
            let tail: Option<syn::Stmt> = head.is_some().then(|| {
                syn::parse_quote! {
                    // ignore unknown field
                    for _ in 0..(len - #count) {
                        cbor4ii::core::dec::IgnoredAny::decode(reader)?;
                    }
                }
            });

            syn::parse_quote! {
                impl<'de> cbor4ii::core::dec::Decode<'de> for #ident {
                    #[inline]
                    fn decode<R: cbor4ii::core::dec::Read<'de>>(reader: &mut R)
                        -> Result<Self, cbor4ii::core::error::DecodeError<R::Error>>
                    {
                        #head;
                        let value = {
                            #(#fields)*
                            #build_struct
                        };
                        #tail
                        Ok(value)
                    }
                }
            }
        }
        Data::Enum(data) => {
            let enum_type = data.variants.iter().filter(|v| !is_unknown(&v.attrs)).fold(
                None,
                |mut sum, next| {
                    let ty = match &next.fields {
                        syn::Fields::Named(_) => EnumType::Struct,
                        syn::Fields::Unnamed(fields) if fields.unnamed.len() == 1 => EnumType::One,
                        syn::Fields::Unit => EnumType::Unit,
                        syn::Fields::Unnamed(_) => {
                            panic!("more than 1 unnamed member field are not allowed")
                        }
                    };
                    match (*sum.get_or_insert(ty), ty) {
                        (EnumType::Struct, EnumType::Struct)
                        | (EnumType::Struct, EnumType::Unit)
                        | (EnumType::Unit, EnumType::Unit)
                        | (EnumType::One, EnumType::One) => (),
                        (EnumType::Unit, EnumType::One)
                        | (EnumType::One, EnumType::Unit)
                        | (_, EnumType::Struct) => sum = Some(EnumType::Struct),
                        _ => panic!("enum member types must be consistent: {:?}", (sum, ty)),
                    }
                    sum
                },
            );
            let enum_type = enum_type.expect("enum cannot be empty");
            let mut iter = data.variants.iter().peekable();

            let unknown_arm: Option<syn::Arm> = iter.next_if(|variant| is_unknown(&variant.attrs))
                .map(|unknown| {
                    let name = &unknown.ident;
                    assert!(
                        unknown.discriminant.is_none(),
                        "unknown member is not allowed custom discriminant"
                    );
                    assert!(
                        is_with(&unknown.attrs).is_none(),
                        "unknown member is not allowed with type"
                    );

                    match &unknown.fields {
                        syn::Fields::Unnamed(fields) => match fields.unnamed.len() {
                            1 => {
                                assert_eq!(enum_type, EnumType::Unit);
                                syn::parse_quote! {
                                    tag => #ident::#name(tag),
                                }
                            }
                            2 => {
                                assert_eq!(enum_type, EnumType::One);
                                let val_ty = &fields.unnamed[1].ty;
                                syn::parse_quote! {
                                    tag => {
                                        let tag: u32 = tag.try_into().map_err(|_| cbor4ii::core::error::DecodeError::CastOverflow {
                                             name: &"unknown-tag",
                                        })?;
                                        let val = <#val_ty as cbor4ii::core::dec::Decode<'_>>::decode(reader)?;
                                        #ident::#name(tag, val)
                                    },
                                }
                            }
                            _ => panic!("unknown member must be a tag and a value"),
                        },
                        _ => panic!("named enum unsupported"),
                    }
                });

            if matches!(enum_type, EnumType::Struct) {
                assert!(
                    unknown_arm.is_none(),
                    "struct enum does not allow unknown variants"
                );
            }

            let mut discriminant: u32 = 0;
            let fields = iter
                .map(|field| -> syn::Arm {
                    match field.discriminant.as_ref() {
                        Some((_, syn::Expr::Lit(syn::ExprLit { lit: syn::Lit::Int(lit), .. }))) => {
                            discriminant = lit.base10_parse::<u32>().unwrap();
                        },
                        Some(_) => panic!("unsupported discriminant type"),
                        None => (),
                    };
                    discriminant += 1;
                    let idx = discriminant as u64;
                    let name = &field.ident;

                    assert!(!is_unknown(&field.attrs), "unknown member must be first");

                    match enum_type {
                        EnumType::Unit => {
                            assert!(is_with(&field.attrs).is_none(), "unit member is not allowed with type");
                            syn::parse_quote!{
                                #idx => #ident::#name,
                            }
                        },
                        EnumType::One => {
                            let val_ty = &field.fields.iter().next().unwrap().ty;
                            let value: syn::Expr = match is_with(&field.attrs) {
                                Some(with_type)
                                    => syn::parse_quote!(<#with_type<#val_ty> as cbor4ii::core::dec::Decode<'_>>::decode(reader)?.0),
                                None => syn::parse_quote!(<#val_ty as cbor4ii::core::dec::Decode<'_>>::decode(reader)?)
                            };

                            syn::parse_quote!{
                                #idx => #ident::#name(#value),
                            }
                        },
                        EnumType::Struct => {
                            let is_named = field.fields.iter().all(|field| field.ident.is_some());
                            let names = field.fields.iter()
                                .enumerate()
                                .map(|(idx, field)| match field.ident.as_ref() {
                                    Some(name) => name.clone(),
                                    None => {
                                        let name = format!("unit{idx}");
                                        syn::Ident::new(&name, field.span())
                                    }
                                })
                                .collect::<Vec<_>>();
                            let count = field.fields.len();

                            let stmt = field.fields.iter()
                                .zip(names.iter())
                                .map(|(field, field_name)| -> syn::Stmt {
                                    let val_ty = &field.ty;
                                    match is_with(&field.attrs) {
                                        Some(with_type) => syn::parse_quote!{
                                            let #field_name = <#with_type<#val_ty> as cbor4ii::core::dec::Decode<'_>>::decode(reader)?.0;
                                        },
                                        None => syn::parse_quote!{
                                            let #field_name = <#val_ty as cbor4ii::core::dec::Decode<'_>>::decode(reader)?;
                                        }
                                    }
                                });
                            let build_struct: syn::Expr = if is_named {
                                syn::parse_quote! { #ident::#name { #(#names),* } }
                            } else {
                                syn::parse_quote! { #ident::#name ( #(#names),* ) }
                            };

                            syn::parse_quote!{
                                #idx => {
                                    let len = cbor4ii::core::types::Array::len(reader)?.unwrap();
                                    let value = {
                                        #(#stmt)*
                                        #build_struct
                                    };

                                    // ignore unknown field
                                    for _ in 0..(len - #count) {
                                        cbor4ii::core::dec::IgnoredAny::decode(reader)?;
                                    }

                                    value
                                },
                            }
                        }
                    }
                });

            let unknown_arm = match unknown_arm {
                Some(arm) => arm,
                None => {
                    syn::parse_quote! {
                        tag => {
                            let err = cbor4ii::core::error::DecodeError::Custom {
                                 name: &stringify!(#ident),
                                 num: tag as u32
                            };
                            return Err(err);
                        }
                    }
                }
            };

            let tag: syn::Stmt = if matches!(enum_type, EnumType::Unit) {
                syn::parse_quote! {
                    let tag = <u64 as cbor4ii::core::dec::Decode<'_>>::decode(reader)?;
                }
            } else {
                syn::parse_quote! {
                    let tag = <cbor4ii::core::types::Tag<()>>::tag(reader)?;
                }
            };

            syn::parse_quote! {
                impl<'de> cbor4ii::core::dec::Decode<'de> for #ident {
                    #[inline]
                    fn decode<R: cbor4ii::core::dec::Read<'de>>(reader: &mut R)
                        -> Result<Self, cbor4ii::core::error::DecodeError<R::Error>>
                    {
                        #tag
                        let value = match tag {
                            #(#fields)*
                            #unknown_arm
                        };
                        Ok(value)
                    }
                }
            }
        }
        Data::Union(_) => panic!("union unsupported"),
    }
}
