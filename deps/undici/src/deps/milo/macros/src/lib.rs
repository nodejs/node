use proc_macro::TokenStream;

mod actions;
mod generators;
mod matchers;
mod native;
mod parser_fields;
mod structs;
mod wasm;

#[proc_macro]
pub fn generate(_: TokenStream) -> TokenStream { generators::generate() }

#[proc_macro]
pub fn case_insensitive_string(input: TokenStream) -> TokenStream { matchers::case_insensitive_string(input) }

#[proc_macro]
pub fn event_with_range(input: TokenStream) -> TokenStream { actions::event_with_range(input) }

#[proc_macro]
pub fn event_with_error(input: TokenStream) -> TokenStream { actions::event_with_error(input) }

#[proc_macro]
pub fn event_with_metadata(input: TokenStream) -> TokenStream { actions::event_with_metadata(input) }

#[proc_macro]
pub fn advance(input: TokenStream) -> TokenStream { actions::advance(input) }

#[proc_macro]
pub fn move_to(input: TokenStream) -> TokenStream { actions::move_to(input) }

#[proc_macro]
pub fn next(_: TokenStream) -> TokenStream { actions::next() }

#[proc_macro]
pub fn suspend(_: TokenStream) -> TokenStream { actions::suspend() }

#[proc_macro]
pub fn fail(input: TokenStream) -> TokenStream { actions::fail(input) }
