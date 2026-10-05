use serde::Serialize;
use syn::ItemFn;

use crate::ast::{
    idents::{FromWithSpan, IntoWithSpan},
    logging::{create_report, AstReport, ContextLocation},
    Attrs, Docs, Ident, LifetimeEnv, Param, PathType, SpanLocation, TypeName,
};

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Debug)]
#[non_exhaustive]
/// Represents a free function not associated with any types.
pub struct Function {
    pub name: Ident,
    pub abi_name: Ident,
    // corresponds to the types in Function(Vec<Box<TypeName>>, Box<TypeName>)
    // the callback type; except here the params aren't anonymous
    pub params: Vec<Param>,
    pub output_type: Option<TypeName>,
    pub lifetimes: LifetimeEnv,
    pub attrs: Attrs,
    pub docs: Docs,
}

impl Function {
    pub(crate) fn from_syn(
        f: &ItemFn,
        parent_attrs: &Attrs,
        module_location: &SpanLocation,
    ) -> Function {
        let ident: Ident = (&f.sig.ident).spanned_into(module_location);
        if let Some(recv) = f.sig.receiver() {
            create_report(AstReport::new(
                "Cannot use self parameter in free function.".into(),
                Some(f.sig.ident.span().spanned_into(module_location)),
                "".into(),
                vec![ContextLocation::new(
                    recv.self_token.span.spanned_into(module_location),
                    "Suggestion: remove self param.".into(),
                )],
            ));
        }

        let mut attrs = parent_attrs.clone();
        attrs.add_attrs(&f.attrs, module_location);

        let concat_func_ident = if attrs.abi_rename.is_empty() {
            format!("diplomat_external_{ident}")
        } else {
            attrs.abi_rename.apply(ident.as_str().into()).to_string()
        };

        let extern_ident = syn::Ident::new(&concat_func_ident, f.sig.ident.span());

        let path_type = PathType::spanned_from(&f.sig, module_location);

        let all_params = f
            .sig
            .inputs
            .iter()
            .filter_map(|a| match a {
                syn::FnArg::Receiver(_) => None,
                syn::FnArg::Typed(ref t) => {
                    Some(Param::from_syn(t, path_type.clone(), module_location))
                }
            })
            .collect::<Vec<_>>();

        let output_type = match &f.sig.output {
            syn::ReturnType::Type(_, return_typ) => Some(TypeName::from_syn(
                return_typ.as_ref(),
                Some(path_type.clone()),
                module_location,
            )),
            syn::ReturnType::Default => None,
        };

        let lifetimes = LifetimeEnv::from_function_item(
            f,
            &all_params[..],
            output_type.as_ref(),
            module_location,
        );

        Self {
            name: ident,
            abi_name: (&extern_ident).spanned_into(module_location),
            params: all_params,
            output_type,
            lifetimes,
            attrs,
            docs: Docs::from_attrs(&f.attrs, module_location),
        }
    }
}

#[cfg(test)]
mod tests {
    use insta;

    use syn;

    use crate::ast::{Attrs, Function};

    #[test]
    fn test_free_function() {
        insta::assert_yaml_snapshot!(Function::from_syn(
            &syn::parse_quote! {
                fn some_func(a : f32, b: f64) {

                }
            },
            &Attrs::default(),
            &crate::ast::SpanLocation::None
        ));
    }

    #[test]
    fn test_free_function_output() {
        insta::assert_yaml_snapshot!(Function::from_syn(
            &syn::parse_quote! {
                fn some_func(a : SomeType) -> Option<()> {

                }
            },
            &Attrs::default(),
            &crate::ast::SpanLocation::None
        ));
    }

    #[test]
    fn test_free_function_lifetimes() {
        insta::assert_yaml_snapshot!(Function::from_syn(
            &syn::parse_quote! {
                fn some_func<'a>(a : &'a SomeType) -> Option<&'a SomeType> {

                }
            },
            &Attrs::default(),
            &crate::ast::SpanLocation::None
        ));
    }
}
