extern crate proc_macro;

use swc_macros_common::prelude::*;
use syn::*;

/// Derives [`From`] for all variants. This only supports an enum where every
/// variant has a single field.
#[proc_macro_derive(FromVariant, attributes(from_variant))]
pub fn derive_from_variant(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = parse::<DeriveInput>(input).expect("failed to parse input as DeriveInput");

    let item = derive(input)
        .into_iter()
        .fold(TokenStream::new(), |mut t, item| {
            item.to_tokens(&mut t);
            t
        });

    print("derive(FromVariant)", item)
}

fn is_ignored(attrs: &[syn::Attribute]) -> bool {
    attrs
        .iter()
        .filter(|attr| attr.path().is_ident("from_variant"))
        .any(|attr| {
            let mut is_unknown = false;
            attr.parse_nested_meta(|meta| {
                is_unknown |= meta.path.is_ident("ignore");
                Ok(())
            })
            .unwrap();
            is_unknown
        })
}

fn derive(
    DeriveInput {
        generics,
        data,
        ident,
        ..
    }: DeriveInput,
) -> Vec<ItemImpl> {
    let variants = match data {
        Data::Enum(DataEnum { variants, .. }) => variants,
        _ => panic!("#[derive(FromVariant)] only works for an enum."),
    };

    let mut from_impls: Vec<ItemImpl> = Vec::new();

    for v in variants {
        if is_ignored(&v.attrs) {
            continue;
        }

        let variant_name = v.ident;
        match v.fields {
            Fields::Unnamed(FieldsUnnamed { unnamed, .. }) => {
                if unnamed.len() != 1 {
                    panic!(
                        "#[derive(FromVariant)] requires all variants to be tuple with exactly \
                         one field"
                    )
                }
                let field = unnamed.into_iter().next().unwrap();

                let variant_type = &field.ty;

                let from_impl: ItemImpl = parse_quote!(
                    impl From<#variant_type> for #ident {
                        fn from(v: #variant_type) -> Self {
                            #ident::#variant_name(v)
                        }
                    }
                );

                let from_impl = from_impl.with_generics(generics.clone());

                from_impls.push(from_impl);
            }
            _ => panic!(
                "#[derive(FromVariant)] requires all variants to be tuple with exactly one field"
            ),
        }
    }

    from_impls
}
