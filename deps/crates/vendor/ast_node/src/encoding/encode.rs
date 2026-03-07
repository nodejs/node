use syn::{spanned::Spanned, Data, DeriveInput};

use super::{is_ignore, is_unknown, is_with, EnumType};

pub fn expand(DeriveInput { ident, data, .. }: DeriveInput) -> syn::ItemImpl {
    match data {
        Data::Struct(data) => {
            let fields = data
                .fields
                .iter()
                .enumerate()
                .filter(|(_, field)| !is_ignore(&field.attrs))
                .map(|(idx, field)| -> syn::Stmt {
                    let fieldpath: syn::ExprField = match field.ident.as_ref() {
                        Some(name) => syn::parse_quote!(self.#name),
                        None => {
                            let name = format!("{idx}");
                            let name = syn::LitInt::new(&name, field.span());
                            syn::parse_quote!(self.#name)
                        }
                    };

                    match is_with(&field.attrs) {
                        Some(with_type) => syn::parse_quote! {
                            cbor4ii::core::enc::Encode::encode(&#with_type(&#fieldpath), writer)?;
                        },
                        None => syn::parse_quote! {
                            cbor4ii::core::enc::Encode::encode(&#fieldpath, writer)?;
                        },
                    }
                });
            let count = data.fields.len();
            let head: Option<syn::Stmt> = (count != 1).then(|| {
                syn::parse_quote! {
                    <cbor4ii::core::types::Array<()>>::bounded(#count, writer)?;
                }
            });

            syn::parse_quote! {
                impl cbor4ii::core::enc::Encode for #ident {
                    #[inline]
                    fn encode<W: cbor4ii::core::enc::Write>(&self, writer: &mut W)
                        -> Result<(), cbor4ii::core::error::EncodeError<W::Error>>
                    {
                        #head
                        #(#fields)*;
                        Ok(())
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
                                    #ident::#name(tag)
                                        => tag.encode(writer)?,
                                }
                            }
                            2 => {
                                assert_eq!(enum_type, EnumType::One);
                                syn::parse_quote! {
                                    #ident::#name(tag, value)
                                        => cbor4ii::core::types::Tag((*tag).into(), &*value).encode(writer)?,
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
            let fields = iter.map(|field| -> syn::Arm {
                match field.discriminant.as_ref() {
                    Some((_, syn::Expr::Lit(syn::ExprLit { lit: syn::Lit::Int(lit), .. }))) => {
                        discriminant = lit.base10_parse::<u32>().unwrap();
                    },
                    Some(_) => panic!("unsupported discriminant type"),
                    None => (),
                };
                discriminant += 1;
                let idx = discriminant;
                let name = &field.ident;

                assert!(
                    !is_unknown(&field.attrs),
                    "unknown member must be first: {:?}",
                    field.attrs.len()
                );

                match enum_type {
                    EnumType::Unit => {
                        assert!(
                            is_with(&field.attrs).is_none(),
                            "unit member is not allowed with type"
                        );

                        syn::parse_quote! {
                            #ident::#name => #idx.encode(writer)?,
                        }
                    },
                    EnumType::One => {
                        let value: syn::Expr = match is_with(&field.attrs) {
                            Some(ty) => syn::parse_quote! {
                                &#ty(&*value)
                            },
                            None => syn::parse_quote! {
                                &*value
                            },
                        };

                        syn::parse_quote! {
                            #ident::#name(value) => {
                                cbor4ii::core::types::Tag(
                                    #idx.into(),
                                    #value
                                ).encode(writer)?;
                            },
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
                        let stmt = field.fields.iter()
                            .zip(names.iter())
                            .map(|(field, field_name)| -> syn::Stmt {
                                match is_with(&field.attrs) {
                                    Some(ty) => syn::parse_quote! {
                                        cbor4ii::core::enc::Encode::encode(&#ty(&*#field_name), writer)?;
                                    },
                                    None => syn::parse_quote! {
                                        cbor4ii::core::enc::Encode::encode(&*#field_name, writer)?;
                                    },
                                }
                            });
                        let count = stmt.len();
                        let head: syn::Expr = syn::parse_quote!{{
                            cbor4ii::core::types::Tag(#idx.into(), cbor4ii::core::types::Nothing).encode(writer)?;
                            cbor4ii::core::types::Array::bounded(#count, writer)?;
                        }};

                        if is_named {
                            syn::parse_quote!{
                                #ident::#name { #(#names),* } => {
                                    #head;
                                    #(#stmt)*
                                },
                            }
                        } else {
                            syn::parse_quote!{
                                #ident::#name ( #(#names),* ) => {
                                    #head;
                                    #(#stmt)*
                                },
                            }
                        }
                    }
                }
            });

            syn::parse_quote! {
                impl cbor4ii::core::enc::Encode for #ident {
                    #[inline]
                    fn encode<W: cbor4ii::core::enc::Write>(&self, writer: &mut W)
                        -> Result<(), cbor4ii::core::error::EncodeError<W::Error>>
                    {
                        match self {
                            #(#fields)*
                            #unknown_arm
                        }
                        Ok(())
                    }
                }
            }
        }
        Data::Union(_) => panic!("union unsupported"),
    }
}
