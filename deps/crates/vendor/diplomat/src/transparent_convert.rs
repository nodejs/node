use quote::quote;
use syn::*;

pub fn gen_transparent_convert(s: ItemStruct) -> proc_macro2::TokenStream {
    let mut fields = s.fields.iter();
    let field1 = if let Some(field1) = fields.next() {
        &field1.ty
    } else {
        panic!("#[diplomat::transparent_convert] only allowed on structs with a single field")
    };

    if fields.next().is_some() {
        panic!("#[diplomat::transparent_convert] only allowed on structs with a single field")
    }
    let struct_name = &s.ident;
    let (impl_generics, ty_generics, _) = s.generics.split_for_impl();
    let mut impl_generics: Generics = parse_quote!(#impl_generics);
    let custom_lifetime: GenericParam = parse_quote!('transparent_convert_outer);
    impl_generics.params.push(custom_lifetime);
    quote! {
        impl #impl_generics #struct_name #ty_generics {
            // can potentially add transparent_convert_owned, _mut later
            pub(crate) fn transparent_convert(from: &'transparent_convert_outer #field1) -> &'transparent_convert_outer Self {
                // Safety: This is safe because the caller of gen_transparent_convert
                // adds a repr(transparent) to the struct.
                unsafe {
                    &*(from as *const #field1 as *const Self)
                }
            }
        }
    }
}
