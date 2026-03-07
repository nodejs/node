use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use swc_macros_common::{access_field, join_stmts};
use syn::{parse_quote, DeriveInput, Expr, Field, Fields, Stmt, Token};

pub fn expand(input: DeriveInput) -> TokenStream {
    match &input.data {
        syn::Data::Struct(s) => {
            let body = call_merge_for_fields(&quote!(self), &s.fields);
            let body = join_stmts(&body);

            let ident = &input.ident;
            parse_quote!(
                #[automatically_derived]
                impl swc_config::merge::Merge for #ident {
                    fn merge(&mut self, _other: Self) {
                        #body
                    }
                }
            )
        }
        syn::Data::Enum(_) => unimplemented!("derive(Merge) does not support an enum"),
        syn::Data::Union(_) => unimplemented!("derive(Merge) does not support a union"),
    }
}

fn call_merge_for_fields(obj: &dyn ToTokens, fields: &Fields) -> Vec<Stmt> {
    fn call_merge(obj: &dyn ToTokens, idx: usize, f: &Field) -> Expr {
        let r = quote!(_other);

        let l = access_field(obj, idx, f);
        let r = access_field(&r, idx, f);
        parse_quote!(swc_config::merge::Merge::merge(&mut #l, #r))
    }

    match fields {
        Fields::Named(fs) => fs
            .named
            .iter()
            .enumerate()
            .map(|(idx, f)| call_merge(obj, idx, f))
            .map(|expr| Stmt::Expr(expr, Some(Token![;](fs.brace_token.span.join()))))
            .collect(),
        Fields::Unnamed(fs) => fs
            .unnamed
            .iter()
            .enumerate()
            .map(|(idx, f)| call_merge(obj, idx, f))
            .map(|expr| Stmt::Expr(expr, Some(Token![;](fs.paren_token.span.join()))))
            .collect(),
        Fields::Unit => unimplemented!("derive(Merge) does not support a unit struct"),
    }
}
