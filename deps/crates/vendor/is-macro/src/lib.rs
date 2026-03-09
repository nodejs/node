extern crate proc_macro;

use heck::ToSnakeCase;
use proc_macro2::Span;
use quote::{quote, ToTokens};
use syn::{
    parse,
    parse::Parse,
    parse2, parse_quote,
    punctuated::{Pair, Punctuated},
    spanned::Spanned,
    Data, DataEnum, DeriveInput, Expr, ExprLit, Field, Fields, Generics, Ident, ImplItem, ItemImpl,
    Lit, Meta, MetaNameValue, Path, Token, Type, TypePath, TypeReference, TypeTuple, WhereClause,
};

/// A proc macro to generate methods like is_variant / expect_variant.
///
///
/// # Example
///
/// ```rust
/// 
/// use is_macro::Is;
/// #[derive(Debug, Is)]
/// pub enum Enum<T> {
///     A,
///     B(T),
///     C(Option<T>),
/// }
///
/// // Rust's type inference cannot handle this.
/// assert!(Enum::<()>::A.is_a());
///
/// assert_eq!(Enum::B(String::from("foo")).b(), Some(String::from("foo")));
///
/// assert_eq!(Enum::B(String::from("foo")).expect_b(), String::from("foo"));
/// ```
///
/// # Renaming
///
/// ```rust
/// 
/// use is_macro::Is;
/// #[derive(Debug, Is)]
/// pub enum Enum {
///     #[is(name = "video_mp4")]
///     VideoMp4,
/// }
///
/// assert!(Enum::VideoMp4.is_video_mp4());
/// ```
#[proc_macro_derive(Is, attributes(is))]
pub fn is(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input: DeriveInput = syn::parse(input).expect("failed to parse derive input");
    let generics: Generics = input.generics.clone();

    let items = match input.data {
        Data::Enum(e) => expand(e),
        _ => panic!("`Is` can be applied only on enums"),
    };

    ItemImpl {
        attrs: vec![],
        defaultness: None,
        unsafety: None,
        impl_token: Default::default(),
        generics: Default::default(),
        trait_: None,
        self_ty: Box::new(Type::Path(TypePath {
            qself: None,
            path: Path::from(input.ident),
        })),
        brace_token: Default::default(),
        items,
    }
    .with_generics(generics)
    .into_token_stream()
    .into()
}

#[derive(Debug)]
struct Input {
    name: String,
}

impl Parse for Input {
    fn parse(input: parse::ParseStream) -> syn::Result<Self> {
        let _: Ident = input.parse()?;
        let _: Token![=] = input.parse()?;

        let name = input.parse::<ExprLit>()?;

        Ok(Input {
            name: match name.lit {
                Lit::Str(s) => s.value(),
                _ => panic!("is(name = ...) expects a string literal"),
            },
        })
    }
}

fn expand(input: DataEnum) -> Vec<ImplItem> {
    let mut items = vec![];

    for v in &input.variants {
        let attrs = v
            .attrs
            .iter()
            .filter(|attr| attr.path().is_ident("is"))
            .collect::<Vec<_>>();
        if attrs.len() >= 2 {
            panic!("derive(Is) expects no attribute or one attribute")
        }
        let i = match attrs.into_iter().next() {
            None => Input {
                name: {
                    v.ident.to_string().to_snake_case()
                    //
                },
            },
            Some(attr) => {
                //

                let mut input = Input {
                    name: Default::default(),
                };

                let mut apply = |v: &MetaNameValue| {
                    assert!(
                        v.path.is_ident("name"),
                        "Currently, is() only supports `is(name = 'foo')`"
                    );

                    input.name = match &v.value {
                        Expr::Lit(ExprLit {
                            lit: Lit::Str(s), ..
                        }) => s.value(),
                        _ => unimplemented!(
                            "is(): name must be a string literal but {:?} is provided",
                            v.value
                        ),
                    };
                };

                match &attr.meta {
                    Meta::NameValue(v) => {
                        //
                        apply(v)
                    }
                    Meta::List(l) => {
                        // Handle is(name = "foo")
                        input = parse2(l.tokens.clone()).expect("failed to parse input");
                    }
                    _ => unimplemented!("is({:?})", attr.meta),
                }

                input
            }
        };

        let name = &*i.name;
        {
            let name_of_is = Ident::new(&format!("is_{name}"), v.ident.span());
            let docs_of_is = format!(
                "Returns `true` if `self` is of variant [`{variant}`].\n\n[`{variant}`]: \
                 #variant.{variant}",
                variant = v.ident,
            );

            let variant = &v.ident;

            let item_impl: ItemImpl = parse_quote!(
                impl Type {
                    #[doc = #docs_of_is]
                    #[inline]
                    pub const fn #name_of_is(&self) -> bool {
                        match *self {
                            Self::#variant { .. } => true,
                            _ => false,
                        }
                    }
                }
            );

            items.extend(item_impl.items);
        }

        {
            let name_of_cast = Ident::new(&format!("as_{name}"), v.ident.span());
            let name_of_cast_mut = Ident::new(&format!("as_mut_{name}"), v.ident.span());
            let name_of_expect = Ident::new(&format!("expect_{name}"), v.ident.span());
            let name_of_take = Ident::new(name, v.ident.span());

            let docs_of_cast = format!(
                "Returns `Some` if `self` is a reference of variant [`{variant}`], and `None` \
                 otherwise.\n\n[`{variant}`]: #variant.{variant}",
                variant = v.ident,
            );
            let docs_of_cast_mut = format!(
                "Returns `Some` if `self` is a mutable reference of variant [`{variant}`], and \
                 `None` otherwise.\n\n[`{variant}`]: #variant.{variant}",
                variant = v.ident,
            );
            let docs_of_expect = format!(
                "Unwraps the value, yielding the content of [`{variant}`].\n\n# Panics\n\nPanics \
                 if the value is not [`{variant}`], with a panic message including the content of \
                 `self`.\n\n[`{variant}`]: #variant.{variant}",
                variant = v.ident,
            );
            let docs_of_take = format!(
                "Returns `Some` if `self` is of variant [`{variant}`], and `None` \
                 otherwise.\n\n[`{variant}`]: #variant.{variant}",
                variant = v.ident,
            );

            if let Fields::Unnamed(fields) = &v.fields {
                let types = fields.unnamed.iter().map(|f| f.ty.clone());
                let cast_ty = types_to_type(types.clone().map(|ty| add_ref(false, ty)));
                let cast_ty_mut = types_to_type(types.clone().map(|ty| add_ref(true, ty)));
                let ty = types_to_type(types);

                let mut fields: Punctuated<Ident, Token![,]> = fields
                    .unnamed
                    .clone()
                    .into_pairs()
                    .enumerate()
                    .map(|(i, pair)| {
                        let handle = |f: Field| {
                            //
                            Ident::new(&format!("v{i}"), f.span())
                        };
                        match pair {
                            Pair::Punctuated(v, p) => Pair::Punctuated(handle(v), p),
                            Pair::End(v) => Pair::End(handle(v)),
                        }
                    })
                    .collect();

                // Make sure that we don't have any trailing punctuation
                // This ensure that if we have a single unnamed field,
                // we will produce a value of the form `(v)`,
                // not a single-element tuple `(v,)`
                if let Some(mut pair) = fields.pop() {
                    if let Pair::Punctuated(v, _) = pair {
                        pair = Pair::End(v);
                    }
                    fields.extend(std::iter::once(pair));
                }

                let variant = &v.ident;

                let item_impl: ItemImpl = parse_quote!(
                    impl #ty {
                        #[doc = #docs_of_cast]
                        #[inline]
                        pub fn #name_of_cast(&self) -> Option<#cast_ty> {
                            match self {
                                Self::#variant(#fields) => Some((#fields)),
                                _ => None,
                            }
                        }

                        #[doc = #docs_of_cast_mut]
                        #[inline]
                        pub fn #name_of_cast_mut(&mut self) -> Option<#cast_ty_mut> {
                            match self {
                                Self::#variant(#fields) => Some((#fields)),
                                _ => None,
                            }
                        }

                        #[doc = #docs_of_expect]
                        #[inline]
                        pub fn #name_of_expect(self) -> #ty
                        where
                            Self: ::std::fmt::Debug,
                        {
                            match self {
                                Self::#variant(#fields) => (#fields),
                                _ => panic!("called expect on {:?}", self),
                            }
                        }

                        #[doc = #docs_of_take]
                        #[inline]
                        pub fn #name_of_take(self) -> Option<#ty> {
                            match self {
                                Self::#variant(#fields) => Some((#fields)),
                                _ => None,
                            }
                        }
                    }
                );

                items.extend(item_impl.items);
            }
        }
    }

    items
}

fn types_to_type(types: impl Iterator<Item = Type>) -> Type {
    let mut types: Punctuated<_, _> = types.collect();
    if types.len() == 1 {
        types.pop().expect("len is 1").into_value()
    } else {
        TypeTuple {
            paren_token: Default::default(),
            elems: types,
        }
        .into()
    }
}

fn add_ref(mutable: bool, ty: Type) -> Type {
    Type::Reference(TypeReference {
        and_token: Default::default(),
        lifetime: None,
        mutability: if mutable {
            Some(Default::default())
        } else {
            None
        },
        elem: Box::new(ty),
    })
}

/// Extension trait for `ItemImpl` (impl block).
trait ItemImplExt {
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
            generics
                .params
                .push_punct(syn::token::Comma(Span::call_site()));
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
            let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();
            let item = if let Some((ref polarity, ref path, ref for_token)) = self.trait_ {
                quote! {
                    impl #impl_generics #polarity #path #for_token #ty #ty_generics #where_clause {}
                }
            } else {
                quote! {
                    impl #impl_generics #ty #ty_generics #where_clause {}

                }
            };
            parse2(item.into_token_stream())
                .unwrap_or_else(|err| panic!("with_generics failed: {}", err))
        };

        // Handle generics added by proc-macro.
        item.generics
            .params
            .extend(self.generics.params.into_pairs());
        match self.generics.where_clause {
            Some(WhereClause {
                ref mut predicates, ..
            }) => predicates.extend(
                generics
                    .where_clause
                    .into_iter()
                    .flat_map(|wc| wc.predicates.into_pairs()),
            ),
            ref mut opt @ None => *opt = generics.where_clause,
        }

        ItemImpl {
            attrs: self.attrs,
            defaultness: self.defaultness,
            unsafety: self.unsafety,
            impl_token: self.impl_token,
            brace_token: self.brace_token,
            items: self.items,
            ..item
        }
    }
}
