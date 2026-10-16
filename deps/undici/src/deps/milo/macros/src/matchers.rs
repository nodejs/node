use proc_macro::TokenStream;
use quote::quote;
use syn::{Expr, LitStr, parse_macro_input, parse_str};

/// Matches a string in case insensitive way.
pub fn case_insensitive_string(input: TokenStream) -> TokenStream {
  let definition = parse_macro_input!(input as LitStr);

  let bytes: Vec<_> = definition
    .value()
    .bytes()
    .map(|b| parse_str::<Expr>(&format!("{} | {}", b.to_ascii_uppercase(), b.to_ascii_lowercase())).unwrap())
    .collect();

  TokenStream::from(quote! { [#(#bytes),*] })
}
