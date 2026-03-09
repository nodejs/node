#![doc(html_root_url = "https://docs.rs/wasm-bindgen-macro/0.2")]

extern crate proc_macro;

use proc_macro::TokenStream;
use quote::quote;

/// A list of all the attributes can be found here: https://wasm-bindgen.github.io/wasm-bindgen/reference/attributes/index.html
#[proc_macro_attribute]
pub fn wasm_bindgen(attr: TokenStream, input: TokenStream) -> TokenStream {
    match wasm_bindgen_macro_support::expand(attr.into(), input.into()) {
        Ok(tokens) => {
            if cfg!(xxx_debug_only_print_generated_code) {
                println!("{tokens}");
            }
            tokens.into()
        }
        Err(diagnostic) => (quote! { #diagnostic }).into(),
    }
}

/// This macro takes a JS module as input and returns a URL that can be used to
/// access it at runtime.
///
/// The module can be specified in a few ways:
/// - You can use `inline_js = "..."` to create an inline JS file.
/// - You can use `module = "/foo/bar"` to reference a file relative to the
///   root of the crate the macro is invoked in.
///
/// The returned URL can be used for things like creating workers/worklets:
/// ```no_run
/// use web_sys::Worker;
/// let worker = Worker::new(&wasm_bindgen::link_to!(module = "/src/worker.js"));
/// ```
#[proc_macro]
pub fn link_to(input: TokenStream) -> TokenStream {
    match wasm_bindgen_macro_support::expand_link_to(input.into()) {
        Ok(tokens) => {
            if cfg!(xxx_debug_only_print_generated_code) {
                println!("{tokens}");
            }
            tokens.into()
        }
        // This `String::clone` is here so that IDEs know this is supposed to be a
        // `String` and can keep type-checking the rest of the program even if the macro
        // fails.
        Err(diagnostic) => (quote! { String::clone(#diagnostic) }).into(),
    }
}

#[proc_macro_attribute]
pub fn __wasm_bindgen_class_marker(attr: TokenStream, input: TokenStream) -> TokenStream {
    match wasm_bindgen_macro_support::expand_class_marker(attr.into(), input.into()) {
        Ok(tokens) => {
            if cfg!(xxx_debug_only_print_generated_code) {
                println!("{tokens}");
            }
            tokens.into()
        }
        Err(diagnostic) => (quote! { #diagnostic }).into(),
    }
}

#[proc_macro_derive(BindgenedStruct, attributes(wasm_bindgen))]
pub fn __wasm_bindgen_struct_marker(item: TokenStream) -> TokenStream {
    match wasm_bindgen_macro_support::expand_struct_marker(item.into()) {
        Ok(tokens) => {
            if cfg!(xxx_debug_only_print_generated_code) {
                println!("{tokens}");
            }
            tokens.into()
        }
        Err(diagnostic) => (quote! { #diagnostic }).into(),
    }
}
