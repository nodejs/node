use proc_macro2::Span;
use quote::quote;
use syn::{
    parse, parse_quote, punctuated::Punctuated, spanned::Spanned, Arm, BinOp, Block, Data,
    DeriveInput, Expr, ExprBinary, ExprBlock, Field, FieldPat, Fields, Ident, Index, Member, Pat,
    PatIdent, PatRest, PatStruct, PatTuple, Path, Stmt, Token,
};

/// Derives `swc_common::TypeEq`.
///
/// - Field annotated with `#[use_eq]` will be compared using `==`.
/// - Field annotated with `#[not_type]` will be ignored
#[proc_macro_derive(TypeEq, attributes(not_type, use_eq, use_eq_ignore_span))]
pub fn derive_type_eq(item: proc_macro::TokenStream) -> proc_macro::TokenStream {
    Deriver {
        trait_name: Ident::new("TypeEq", Span::call_site()),
        method_name: Ident::new("type_eq", Span::call_site()),
        ignore_field: Box::new(|field| {
            // Search for `#[not_type]`.
            for attr in &field.attrs {
                if attr.path().is_ident("not_type") {
                    return true;
                }
            }

            false
        }),
    }
    .derive(item)
}

/// Derives `swc_common::EqIgnoreSpan`.
///
///
/// Fields annotated with `#[not_spanned]` or `#[use_eq]` will use` ==` instead
/// of `eq_ignore_span`.
#[proc_macro_derive(EqIgnoreSpan, attributes(not_spanned, use_eq))]
pub fn derive_eq_ignore_span(item: proc_macro::TokenStream) -> proc_macro::TokenStream {
    Deriver {
        trait_name: Ident::new("EqIgnoreSpan", Span::call_site()),
        method_name: Ident::new("eq_ignore_span", Span::call_site()),
        ignore_field: Box::new(|_field| {
            // We call eq_ignore_span for all fields.
            false
        }),
    }
    .derive(item)
}

struct Deriver {
    trait_name: Ident,
    method_name: Ident,
    ignore_field: Box<dyn Fn(&Field) -> bool>,
}

impl Deriver {
    fn derive(&self, item: proc_macro::TokenStream) -> proc_macro::TokenStream {
        let input: DeriveInput = parse(item).unwrap();

        let body = self.make_body(&input.data);

        let trait_name = &self.trait_name;
        let ty = &input.ident;
        let method_name = &self.method_name;
        quote!(
            #[automatically_derived]
            impl ::swc_common::#trait_name for #ty {
                #[allow(non_snake_case)]
                fn #method_name(&self, other: &Self) -> bool {
                    #body
                }
            }
        )
        .into()
    }

    fn make_body(&self, data: &Data) -> Expr {
        match data {
            Data::Struct(s) => {
                let arm = self.make_arm_from_fields(parse_quote!(Self), &s.fields);

                parse_quote!(match (self, other) { #arm })
            }
            Data::Enum(e) => {
                //
                let mut arms = Punctuated::<_, Token![,]>::default();
                for v in &e.variants {
                    let vi = &v.ident;
                    let arm = self.make_arm_from_fields(parse_quote!(Self::#vi), &v.fields);

                    arms.push(arm);
                }

                arms.push(parse_quote!(_ => false));

                parse_quote!(match (self, other) { #arms })
            }
            Data::Union(_) => unimplemented!("union"),
        }
    }

    fn make_arm_from_fields(&self, pat_path: Path, fields: &Fields) -> Arm {
        let mut l_pat_fields = Punctuated::<_, Token![,]>::default();
        let mut r_pat_fields = Punctuated::<_, Token![,]>::default();
        let mut exprs = Vec::new();

        for (i, field) in fields
            .iter()
            .enumerate()
            .filter(|(_, f)| !(self.ignore_field)(f))
        {
            let method_name =
                if field.attrs.iter().any(|attr| {
                    attr.path().is_ident("not_spanned") || attr.path().is_ident("use_eq")
                }) {
                    Ident::new("eq", Span::call_site())
                } else if field
                    .attrs
                    .iter()
                    .any(|attr| attr.path().is_ident("use_eq_ignore_span"))
                {
                    Ident::new("eq_ignore_span", Span::call_site())
                } else {
                    self.method_name.clone()
                };

            let base = field
                .ident
                .clone()
                .unwrap_or_else(|| Ident::new(&format!("_{i}"), field.ty.span()));
            //
            let l_binding_ident = Ident::new(&format!("_l_{base}"), base.span());
            let r_binding_ident = Ident::new(&format!("_r_{base}"), base.span());

            let make_pat_field = |ident: &Ident| FieldPat {
                attrs: Default::default(),
                member: match &field.ident {
                    Some(v) => Member::Named(v.clone()),
                    None => Member::Unnamed(Index {
                        index: i as _,
                        span: field.ty.span(),
                    }),
                },
                colon_token: Some(Token![:](ident.span())),
                pat: Box::new(Pat::Ident(PatIdent {
                    attrs: Default::default(),
                    by_ref: Some(Token![ref](ident.span())),
                    mutability: None,
                    ident: ident.clone(),
                    subpat: None,
                })),
            };

            l_pat_fields.push(make_pat_field(&l_binding_ident));
            r_pat_fields.push(make_pat_field(&r_binding_ident));

            exprs.push(parse_quote!(#l_binding_ident.#method_name(#r_binding_ident)));
        }

        // true && a.type_eq(&other.a) && b.type_eq(&other.b)
        let mut expr: Expr = parse_quote!(true);

        for expr_el in exprs {
            expr = Expr::Binary(ExprBinary {
                attrs: Default::default(),
                left: Box::new(expr),
                op: BinOp::And(Token![&&](Span::call_site())),
                right: Box::new(expr_el),
            });
        }

        Arm {
            attrs: Default::default(),
            pat: Pat::Tuple(PatTuple {
                attrs: Default::default(),
                paren_token: Default::default(),
                elems: {
                    let mut elems = Punctuated::default();
                    elems.push(Pat::Struct(PatStruct {
                        attrs: Default::default(),
                        qself: None,
                        path: pat_path.clone(),
                        brace_token: Default::default(),
                        fields: l_pat_fields,
                        rest: Some(PatRest {
                            attrs: Default::default(),
                            dot2_token: Token![..](Span::call_site()),
                        }),
                    }));
                    elems.push(Pat::Struct(PatStruct {
                        attrs: Default::default(),
                        qself: None,
                        path: pat_path,
                        brace_token: Default::default(),
                        fields: r_pat_fields,
                        rest: Some(PatRest {
                            attrs: Default::default(),
                            dot2_token: Token![..](Span::call_site()),
                        }),
                    }));
                    elems
                },
            }),
            guard: Default::default(),
            fat_arrow_token: Token![=>](Span::call_site()),
            body: Box::new(Expr::Block(ExprBlock {
                attrs: Default::default(),
                label: Default::default(),
                block: Block {
                    brace_token: Default::default(),
                    stmts: vec![Stmt::Expr(expr, None)],
                },
            })),
            comma: Default::default(),
        }
    }
}
