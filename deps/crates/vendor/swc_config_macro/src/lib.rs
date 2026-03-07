extern crate proc_macro;

mod merge;

#[proc_macro_derive(Merge)]
pub fn derive_merge(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = syn::parse(input).expect("failed to parse input as DeriveInput");

    self::merge::expand(input).into()
}
