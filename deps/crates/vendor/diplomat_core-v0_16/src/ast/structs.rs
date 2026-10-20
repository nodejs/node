use serde::Serialize;
use syn::spanned::Spanned;

use crate::ast::idents::{IntoWithSpan, SpanLocation};
use crate::ast::logging::{create_report, AstReport, ContextLocation};

use super::docs::Docs;
use super::{Attrs, Ident, LifetimeEnv, Method, PathType, TypeName};

/// A struct declaration in an FFI module that is not opaque.
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Debug)]
#[non_exhaustive]
pub struct Struct {
    pub name: Ident,
    pub docs: Docs,
    pub lifetimes: LifetimeEnv,
    pub fields: Vec<(Ident, TypeName, Docs, Attrs)>,
    pub methods: Vec<Method>,
    pub output_only: bool,
    pub attrs: Attrs,
}

impl Struct {
    /// Extract a [`Struct`] metadata value from an AST node.
    pub fn new(
        strct: &syn::ItemStruct,
        output_only: bool,
        parent_attrs: &Attrs,
        module_location: &SpanLocation,
    ) -> Self {
        let self_path_type = PathType::extract_self_type(strct, module_location);
        let fields: Vec<_> = strct
            .fields
            .iter()
            .map(|field| {
                // Non-opaque tuple structs will never be allowed
                let name = field
                    .ident
                    .as_ref()
                    .map(|i| i.spanned_into(module_location))
                    .unwrap_or_else(|| {
                        create_report(AstReport::new(
                            "Non-opaque tuple structs are disallowed".into(),
                            Some(field.ty.span().spanned_into(module_location)),
                            "If struct is not opaque, fields must have idents".into(),
                            vec![ContextLocation::new(
                                strct.ident.span().spanned_into(module_location),
                                "Suggestion: mark with #[diplomat::opaque]".into(),
                            )],
                        ));
                    });
                let type_name =
                    TypeName::from_syn(&field.ty, Some(self_path_type.clone()), module_location);
                let docs = Docs::from_attrs(&field.attrs, module_location);

                (
                    name,
                    type_name,
                    docs,
                    Attrs::from_attrs(&field.attrs, module_location),
                )
            })
            .collect();

        let lifetimes = LifetimeEnv::from_struct_item(strct, &fields[..], module_location);
        let mut attrs = parent_attrs.clone();
        attrs.add_attrs(&strct.attrs, module_location);
        Struct {
            name: (&strct.ident).spanned_into(module_location),
            docs: Docs::from_attrs(&strct.attrs, module_location),
            lifetimes,
            fields,
            methods: vec![],
            output_only,
            attrs,
        }
    }
}

#[cfg(test)]
mod tests {
    use insta::{self, Settings};

    use syn;

    use crate::ast::idents::SpanLocation;

    use super::Struct;

    #[test]
    fn simple_struct() {
        let mut settings = Settings::new();
        settings.set_sort_maps(true);

        settings.bind(|| {
            insta::assert_yaml_snapshot!(Struct::new(
                &syn::parse_quote! {
                    /// Some docs.
                    #[diplomat::rust_link(foo::Bar, Struct)]
                    struct MyLocalStruct {
                        a: i32,
                        b: Box<MyLocalStruct>
                    }
                },
                true,
                &Default::default(),
                &SpanLocation::None
            ));
        });
    }
}
