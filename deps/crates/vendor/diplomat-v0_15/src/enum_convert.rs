use quote::{quote, ToTokens};
use syn::parse::{Parse, ParseStream};
use syn::*;

// An attribute that is a list of idents
pub struct EnumConvertAttribute {
    path: Path,

    needs_wildcard: bool,
}

impl Parse for EnumConvertAttribute {
    fn parse(input: ParseStream) -> Result<Self> {
        let paths = input.parse_terminated(Path::parse, Token![,])?;
        if paths.is_empty() {
            return Err(input.error("#[diplomat::enum_convert] needs a path argument"));
        }
        let needs_wildcard = if paths.len() == 2 {
            if let Some(ident) = paths[1].get_ident() {
                if ident == "needs_wildcard" {
                    true
                } else {
                    return Err(input.error(
                        "#[diplomat::enum_convert] only recognizes needs_wildcard keyword",
                    ));
                }
            } else {
                return Err(
                    input.error("#[diplomat::enum_convert] only recognizes needs_wildcard keyword")
                );
            }
        } else if paths.len() > 1 {
            return Err(input.error("#[diplomat::enum_convert] only supports up to two arguments"));
        } else {
            // no needs_wildcard marker
            false
        };
        Ok(EnumConvertAttribute {
            path: paths[0].clone(),
            needs_wildcard,
        })
    }
}

pub fn gen_enum_convert(attr: EnumConvertAttribute, input: ItemEnum) -> proc_macro2::TokenStream {
    let mut from_arms = vec![];
    let mut into_arms = vec![];

    let this_name = &input.ident;
    let other_name = &attr.path;
    for variant in &input.variants {
        if variant.fields != Fields::Unit {
            return Error::new(variant.ident.span(), "variant may not have fields")
                .to_compile_error();
        }

        let variant_name = &variant.ident;
        from_arms.push(quote!(#other_name::#variant_name => Self::#variant_name));
        into_arms.push(quote!(#this_name::#variant_name => Self::#variant_name));
    }

    if attr.needs_wildcard {
        let error = format!(
            "Encountered unknown field for {}",
            other_name.to_token_stream()
        );
        from_arms.push(quote!(_ => unreachable!(#error)))
    }
    quote! {
        #[allow(deprecated)]
        impl From<#other_name> for #this_name {
            fn from(other: #other_name) -> Self {
                match other {
                    #(#from_arms,)*
                }
            }
        }
        #[allow(deprecated)]
        impl From<#this_name> for #other_name {
            fn from(this: #this_name) -> Self {
                match this {
                    #(#into_arms,)*
                }
            }
        }
    }
}
