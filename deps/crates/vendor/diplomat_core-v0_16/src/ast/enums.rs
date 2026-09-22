use serde::Serialize;

use crate::ast::idents::IntoWithSpan;
use crate::ast::logging::create_simple_report;
use crate::ast::SpanLocation;

use super::docs::Docs;
use super::{AttrInheritContext, Attrs, Ident, Method};
use quote::ToTokens;

/// A fieldless enum declaration in an FFI module.
#[derive(Clone, Serialize, Debug, Hash, PartialEq, Eq)]
#[non_exhaustive]
pub struct Enum {
    pub name: Ident,
    pub docs: Docs,
    /// A list of variants of the enum. (name, discriminant, docs, attrs)
    pub variants: Vec<(Ident, isize, Docs, Attrs)>,
    pub methods: Vec<Method>,
    pub attrs: Attrs,
}

impl Enum {
    /// Extract an [`Enum`] metadata value from an AST node.
    pub fn new(enm: &syn::ItemEnum, parent_attrs: &Attrs, module_location: &SpanLocation) -> Enum {
        let mut last_discriminant = -1;
        if !enm.generics.params.is_empty() {
            // Generic types are not allowed.
            // Assuming all enums cannot have lifetimes? We don't even have a
            // `lifetimes` field. If we change our minds we can adjust this later
            // and update the `CustomType::lifetimes` API accordingly.
            create_simple_report(
                (&enm.ident).spanned_into(module_location),
                "Enums cannot have generic parameters.".into(),
                "Suggestion: remove generics".into(),
            );
        }

        let mut attrs = parent_attrs.clone();
        attrs.add_attrs(&enm.attrs, module_location);
        let variant_parent_attrs = attrs.attrs_for_inheritance(AttrInheritContext::Variant);

        Enum {
            name: (&enm.ident).spanned_into(module_location),
            docs: Docs::from_attrs(&enm.attrs, module_location),
            variants: enm
                .variants
                .iter()
                .map(|v| {
                    if !matches!(v.fields, syn::Fields::Unit) {
                        create_simple_report(
                            (&v.ident).spanned_into(module_location),
                            "Enums cannot have fields, we only support C-like enums".into(),
                            "Remove field variant".into(),
                        );
                    }
                    let new_discriminant = v
                        .discriminant
                        .as_ref()
                        .map(|d| {
                            // Reparsing, signed literals are represented
                            // as a negation expression
                            let lit: Result<syn::Lit, _> = syn::parse2(d.1.to_token_stream());
                            if let Ok(syn::Lit::Int(ref lit_int)) = lit {
                                lit_int.base10_parse::<isize>().unwrap()
                            } else {
                                create_simple_report(
                                    (&v.ident).spanned_into(module_location),
                                    "Enum discriminants must be constant integers".into(),
                                    "Expected discriminant to be a constant integer".into(),
                                );
                            }
                        })
                        .unwrap_or_else(|| last_discriminant + 1);

                    last_discriminant = new_discriminant;
                    let mut v_attrs = variant_parent_attrs.clone();
                    v_attrs.add_attrs(&v.attrs, module_location);
                    (
                        (&v.ident).spanned_into(module_location),
                        new_discriminant,
                        Docs::from_attrs(&v.attrs, module_location),
                        v_attrs,
                    )
                })
                .collect(),
            methods: vec![],
            attrs,
        }
    }
}

#[cfg(test)]
mod tests {
    use insta::{self, Settings};

    use syn;

    use super::Enum;

    #[test]
    fn simple_enum() {
        let mut settings = Settings::new();
        settings.set_sort_maps(true);

        settings.bind(|| {
            insta::assert_yaml_snapshot!(Enum::new(
                &syn::parse_quote! {
                    /// Some docs.
                    #[diplomat::rust_link(foo::Bar, Enum)]
                    enum MyLocalEnum {
                        Abc,
                        /// Some more docs.
                        Def
                    }
                },
                &Default::default(),
                &crate::ast::SpanLocation::None
            ));
        });
    }

    #[test]
    fn enum_with_discr() {
        let mut settings = Settings::new();
        settings.set_sort_maps(true);

        settings.bind(|| {
            insta::assert_yaml_snapshot!(Enum::new(
                &syn::parse_quote! {
                    /// Some docs.
                    #[diplomat::rust_link(foo::Bar, Enum)]
                    enum DiscriminantedEnum {
                        Abc = -1,
                        Def = 0,
                        Ghi = 1,
                        Jkl = 2,
                    }
                },
                &Default::default(),
                &crate::ast::SpanLocation::None
            ));
        });
    }
}
