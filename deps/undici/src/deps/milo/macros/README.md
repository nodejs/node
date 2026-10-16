# milo-macros

milo-macros is an internal crate which represents the core of Milo.

It leverages Rust's [procedural macro], [syn] and [quote] crates to allow an easy definition of states and matchers for the parser.

To define a new macro, you have to define a new method which the following signature:

```rust
#[proc_macro]
pub fn new_macro(input: TokenStream) -> TokenStream {
  TokenStream::new( /* ... */)
}
```

Everything that is not included **within** the return value will not be present in the executed code. Macro performance is not important for runtime because macros are only evaluated at compile time.

The `generate!` macro also emits parser constants and internal byte lookup tables used by syntax validators. These tables are generated at compile time and are not public API.

To return a new code, leverage the `quote!` macro, while for parsing use the `parse_macro_input!` macro. Here's an example:

```rust
#[proc_macro]
pub fn char(input: TokenStream) -> TokenStream {
  let character = parse_macro_input!(input as LitChar);
  let byte = LitByte::new(c_uchar::try_from(character.value()).unwrap(), character.span());

  TokenStream::from(quote! { #byte })
}
```

[procedural macro]: https://doc.rust-lang.org/reference/procedural-macros.html
[syn]: https://crates.io/crates/syn
[quote]: https://crates.io/crates/quote
