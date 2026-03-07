use std::iter;

use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::{punctuated::Pair, *};

use crate::def_site;

mod generics;

/// Generics of derived impl item.
#[derive(Debug, Clone)]
pub struct Derive<'a> {
    input: &'a DeriveInput,
    out: ItemImpl,
}

impl<'a> Derive<'a> {
    pub fn new(input: &'a DeriveInput) -> Self {
        let (generics, self_ty) = {
            // Generics for impl cannot have default.
            let params = input
                .generics
                .params
                .clone()
                .into_pairs()
                .map(|mut pair| {
                    if let GenericParam::Type(ref mut t) = *pair.value_mut() {
                        t.eq_token = None;
                        t.default = None;
                    }

                    pair
                })
                .collect();

            let generics = Generics {
                params,
                gt_token: input.generics.gt_token,
                lt_token: input.generics.lt_token,
                where_clause: input.generics.where_clause.clone(),
            };

            // Handle generic declared on type.
            let ty: Box<Type> = {
                let (_, ty_generics, _) = input.generics.split_for_impl();
                let mut t = TokenStream::new();
                input.ident.to_tokens(&mut t);
                ty_generics.to_tokens(&mut t);
                Box::new(
                    parse2(t.into_token_stream())
                        .unwrap_or_else(|err| panic!("failed to parse type: {err}")),
                )
            };

            (generics, ty)
        };

        Derive {
            input,
            out: ItemImpl {
                attrs: Vec::new(),
                impl_token: Token!(impl)(def_site()),
                brace_token: Default::default(),
                defaultness: None,
                unsafety: None,
                generics,
                trait_: None,
                self_ty,
                items: Default::default(),
            },
        }
    }

    /// Set `defaultness`
    pub fn defaultness(&mut self, defaultness: Option<token::Default>) {
        self.out.defaultness = defaultness;
    }

    /// Set `unsafety`
    pub fn unsafety(&mut self, unsafety: Option<token::Unsafe>) {
        self.out.unsafety = unsafety;
    }

    pub fn input(&self) -> &DeriveInput {
        self.input
    }

    pub fn append_to(mut self, item: ItemImpl) -> ItemImpl {
        assert_eq!(self.out.trait_, None);
        if !self.out.generics.params.empty_or_trailing() {
            self.out.generics.params.push_punct(Token![,](def_site()));
        }

        self.out
            .generics
            .params
            .extend(item.generics.params.into_pairs());

        self.out.trait_ = item.trait_;

        self.out.attrs.extend(item.attrs);
        self.out.items.extend(item.items);

        self.out
    }
}
