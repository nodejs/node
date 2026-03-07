use quote::quote;
use syn::{punctuated::Pair, *};

use crate::def_site;

/// Extension trait for `ItemImpl` (impl block).
pub trait ItemImplExt {
    /// Instead of
    ///
    /// ```rust,ignore
    /// let (impl_generics, ty_generics, where_clause) = input.generics.split_for_impl();
    ///
    /// let item: Item = Quote::new(def_site::<Span>())
    ///     .quote_with(smart_quote!(
    /// Vars {
    /// Type: type_name,
    /// impl_generics,
    /// ty_generics,
    /// where_clause,
    /// },
    /// {
    /// impl impl_generics ::swc_common::AstNode for Type ty_generics
    /// where_clause {}
    /// }
    /// )).parse();
    /// ```
    ///
    /// You can use this like
    ///
    /// ```rust,ignore
    // let item = Quote::new(def_site::<Span>())
    ///     .quote_with(smart_quote!(Vars { Type: type_name }, {
    ///         impl ::swc_common::AstNode for Type {}
    ///     }))
    ///     .parse::<ItemImpl>()
    ///     .with_generics(input.generics);
    /// ```
    fn with_generics(self, generics: Generics) -> Self;
}

impl ItemImplExt for ItemImpl {
    fn with_generics(mut self, mut generics: Generics) -> Self {
        // TODO: Check conflicting name

        let need_new_punct = !generics.params.empty_or_trailing();
        if need_new_punct {
            generics.params.push_punct(Token![,](def_site()));
        }

        // Respan
        if let Some(t) = generics.lt_token {
            self.generics.lt_token = Some(t)
        }
        if let Some(t) = generics.gt_token {
            self.generics.gt_token = Some(t)
        }

        let ty = self.self_ty;

        // Handle generics defined on struct, enum, or union.
        let mut item: ItemImpl = {
            let (_, ty_generics, _) = generics.split_for_impl();
            let item = quote! {
                #ty #ty_generics
            };

            ItemImpl {
                generics,
                self_ty: parse2(item).unwrap(),
                ..self
            }
        };

        // Handle generics added by proc-macro.
        item.generics
            .params
            .extend(self.generics.params.into_pairs());

        item
    }
}

pub trait PairExt<T, P>: Sized + Into<Pair<T, P>> {
    fn map_item<F, NewItem>(self, op: F) -> Pair<NewItem, P>
    where
        F: FnOnce(T) -> NewItem,
    {
        match self.into() {
            Pair::Punctuated(t, p) => Pair::Punctuated(op(t), p),
            Pair::End(t) => Pair::End(op(t)),
        }
    }
}

impl<T, P> PairExt<T, P> for Pair<T, P> {}
