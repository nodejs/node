use std::borrow::Cow;
use std::collections::{BTreeMap, BTreeSet};
use syn::parse_quote;
use syn::visit_mut::{self, VisitMut};
use syn::{visit::Visit, Ident, Type};

use crate::error::Diagnostic;

/// Visitor to replace wasm bindgen generics with their concrete types
/// The concrete type is the default type on the import if specified when it was defined.
struct GenericRenameVisitor<'a> {
    renames: &'a BTreeMap<&'a Ident, Option<Cow<'a, syn::Type>>>,
    err: Option<Diagnostic>,
}

impl<'a> VisitMut for GenericRenameVisitor<'a> {
    fn visit_type_mut(&mut self, ty: &mut Type) {
        if self.err.is_some() {
            return;
        }
        if let Type::Path(type_path) = ty {
            // Handle <T as Trait>::AssocType
            if let Some(qself) = &mut type_path.qself {
                if let Type::Path(qself_path) = &mut *qself.ty {
                    if qself_path.qself.is_none() && qself_path.path.segments.len() == 1 {
                        let ident = &qself_path.path.segments[0].ident;
                        if let Some((_, concrete)) = self.renames.get_key_value(ident) {
                            *qself.ty = if let Some(concrete) = concrete {
                                concrete.clone().into_owned()
                            } else {
                                parse_quote! { JsValue }
                            };
                            return;
                        }
                    }
                }
            }
            // Normal T::...
            if type_path.qself.is_none() && !type_path.path.segments.is_empty() {
                let first_seg = &type_path.path.segments[0];

                if let Some((_, concrete)) = self.renames.get_key_value(&first_seg.ident) {
                    if let Some(concrete) = concrete {
                        if type_path.path.segments.len() == 1 {
                            *ty = concrete.clone().into_owned();
                        } else if let Type::Path(concrete_path) = concrete.as_ref() {
                            let remaining: Vec<_> =
                                type_path.path.segments.iter().skip(1).cloned().collect();
                            type_path.path.segments = concrete_path.path.segments.clone();
                            type_path.path.segments.extend(remaining);
                        }
                    } else {
                        *ty = parse_quote! { JsValue };
                    }
                    return;
                }
            }
        }
        visit_mut::visit_type_mut(self, ty);
    }
}

/// Helper visitor for generic parameter usage
#[derive(Debug)]
pub struct GenericNameVisitor<'a, 'b> {
    generic_params: &'a Vec<&'a Ident>,
    /// The generic params that were found
    found_set: &'b mut BTreeSet<Ident>,
}

/// Helper visitor for generic parameter usage
impl<'a, 'b> GenericNameVisitor<'a, 'b> {
    /// Construct a new generic name visitors with a param search set,
    /// and optionally a second parameter search set.
    pub fn new(generic_params: &'a Vec<&'a Ident>, found_set: &'b mut BTreeSet<Ident>) -> Self {
        Self {
            generic_params,
            found_set,
        }
    }
}

impl<'a, 'b> Visit<'a> for GenericNameVisitor<'a, 'b> {
    fn visit_type_reference(&mut self, type_ref: &'a syn::TypeReference) {
        if let syn::Type::Path(type_path) = &*type_ref.elem {
            // Handle <T as Trait>::AssocType - visit the qself type
            if let Some(qself) = &type_path.qself {
                syn::visit::visit_type(self, &qself.ty);
                // Also visit the path segments for any generic args
                for segment in &type_path.path.segments {
                    syn::visit::visit_path_segment(self, segment);
                }
                return;
            }

            if let Some(first_segment) = type_path.path.segments.first() {
                if type_path.path.segments.len() == 1 && first_segment.arguments.is_empty() {
                    if self.generic_params.contains(&&first_segment.ident) {
                        self.found_set.insert(first_segment.ident.clone());
                        return;
                    }
                } else {
                    if self.generic_params.contains(&&first_segment.ident) {
                        self.found_set.insert(first_segment.ident.clone());
                    }

                    syn::visit::visit_path_arguments(self, &first_segment.arguments);

                    for segment in type_path.path.segments.iter().skip(1) {
                        syn::visit::visit_path_segment(self, segment);
                    }
                    return;
                }
            }
        }

        // For other cases, continue normal visiting
        syn::visit::visit_type_reference(self, type_ref);
    }

    fn visit_path(&mut self, path: &'a syn::Path) {
        if let Some(first_segment) = path.segments.first() {
            if self.generic_params.contains(&&first_segment.ident) {
                self.found_set.insert(first_segment.ident.clone());
            }
        }

        for segment in &path.segments {
            match &segment.arguments {
                syn::PathArguments::AngleBracketed(args) => {
                    for arg in &args.args {
                        match arg {
                            syn::GenericArgument::Type(ty) => {
                                syn::visit::visit_type(self, ty);
                            }
                            syn::GenericArgument::AssocType(binding) => {
                                // Don't visit binding.ident, only visit binding.ty
                                syn::visit::visit_type(self, &binding.ty);
                            }
                            _ => {
                                syn::visit::visit_generic_argument(self, arg);
                            }
                        }
                    }
                }
                syn::PathArguments::Parenthesized(args) => {
                    // Handle function syntax like FnMut(T) -> Result<R, JsValue>
                    for input in &args.inputs {
                        syn::visit::visit_type(self, input);
                    }
                    if let syn::ReturnType::Type(_, return_type) = &args.output {
                        syn::visit::visit_type(self, return_type);
                    }
                }
                syn::PathArguments::None => {}
            }
        }
    }
}

/// Obtain the generic parameters and their optional defaults
pub(crate) fn generic_params(generics: &syn::Generics) -> Vec<(&Ident, Option<&syn::Type>)> {
    generics
        .type_params()
        .map(|tp| (&tp.ident, tp.default.as_ref()))
        .collect()
}

/// Returns a vector of token streams representing generic type parameters with their bounds.
/// For example, `<T: Clone, U: Display>` returns `[quote!(T: Clone), quote!(U: Display)]`.
/// This is useful for constructing impl blocks that need to add lifetimes while preserving bounds.
pub(crate) fn type_params_with_bounds(generics: &syn::Generics) -> Vec<proc_macro2::TokenStream> {
    generics
        .type_params()
        .map(|tp| {
            let ident = &tp.ident;
            let bounds = &tp.bounds;
            if bounds.is_empty() {
                quote::quote! { #ident }
            } else {
                quote::quote! { #ident: #bounds }
            }
        })
        .collect()
}
/// Obtain the generic bounds, both inline and where clauses together
pub(crate) fn generic_bounds<'a>(generics: &'a syn::Generics) -> Vec<Cow<'a, syn::WherePredicate>> {
    let mut bounds = Vec::new();
    for param in &generics.params {
        if let syn::GenericParam::Type(type_param) = param {
            if !type_param.bounds.is_empty() {
                let ident = &type_param.ident;
                let predicate = syn::WherePredicate::Type(syn::PredicateType {
                    lifetimes: None,
                    bounded_ty: syn::parse_quote!(#ident),
                    colon_token: syn::Token![:](proc_macro2::Span::call_site()),
                    bounds: type_param.bounds.clone(),
                });
                bounds.push(Cow::Owned(predicate));
            }
        }
    }
    if let Some(where_clause) = &generics.where_clause {
        bounds.extend(where_clause.predicates.iter().map(Cow::Borrowed));
    }
    bounds
}

/// Replace specified lifetime parameters with 'static.
/// This is used when generating concrete ABI types for extern blocks,
/// which cannot have lifetime parameters from the outer scope.
/// Only the lifetimes in `lifetimes_to_staticize` are replaced.
pub(crate) fn staticize_lifetimes(
    mut ty: syn::Type,
    lifetimes_to_staticize: &[&syn::Lifetime],
) -> syn::Type {
    struct LifetimeStaticizer<'a> {
        lifetimes: &'a [&'a syn::Lifetime],
    }
    impl VisitMut for LifetimeStaticizer<'_> {
        fn visit_lifetime_mut(&mut self, lifetime: &mut syn::Lifetime) {
            if self.lifetimes.iter().any(|lt| lt.ident == lifetime.ident) {
                *lifetime = syn::Lifetime::new("'static", lifetime.span());
            }
        }
    }
    LifetimeStaticizer {
        lifetimes: lifetimes_to_staticize,
    }
    .visit_type_mut(&mut ty);
    ty
}

/// Obtain the generic type parameter names
pub(crate) fn generic_param_names(generics: &syn::Generics) -> Vec<&Ident> {
    generics.type_params().map(|tp| &tp.ident).collect()
}

/// Obtain all lifetime parameters from generics
pub(crate) fn lifetime_params(generics: &syn::Generics) -> Vec<&syn::Lifetime> {
    generics.lifetimes().map(|lp| &lp.lifetime).collect()
}

/// Obtain both lifetime and type parameter names from generics
pub(crate) fn all_param_names(generics: &syn::Generics) -> (Vec<&syn::Lifetime>, Vec<&Ident>) {
    (lifetime_params(generics), generic_param_names(generics))
}

/// Helper visitor for lifetime usage detection in types
pub struct LifetimeVisitor<'a> {
    lifetime_params: &'a [&'a syn::Lifetime],
    found_set: BTreeSet<syn::Lifetime>,
}

impl<'a> LifetimeVisitor<'a> {
    pub fn new(lifetime_params: &'a [&'a syn::Lifetime]) -> Self {
        Self {
            lifetime_params,
            found_set: BTreeSet::new(),
        }
    }

    pub fn into_found(self) -> BTreeSet<syn::Lifetime> {
        self.found_set
    }
}

impl<'ast> syn::visit::Visit<'ast> for LifetimeVisitor<'_> {
    fn visit_lifetime(&mut self, lifetime: &'ast syn::Lifetime) {
        if self.lifetime_params.contains(&lifetime) {
            self.found_set.insert(lifetime.clone());
        }
    }
}

/// Find all lifetimes from the given set that are used in a type
pub(crate) fn used_lifetimes_in_type<'a>(
    ty: &syn::Type,
    lifetime_params: &'a [&'a syn::Lifetime],
) -> BTreeSet<syn::Lifetime> {
    let mut visitor = LifetimeVisitor::new(lifetime_params);
    syn::visit::Visit::visit_type(&mut visitor, ty);
    visitor.into_found()
}

pub(crate) fn uses_generic_params(ty: &syn::Type, generic_names: &Vec<&Ident>) -> bool {
    let mut found_set = Default::default();
    let mut visitor = GenericNameVisitor::new(generic_names, &mut found_set);
    visitor.visit_type(ty);
    !found_set.is_empty()
}

pub(crate) fn uses_lifetime_params(ty: &syn::Type, lifetime_params: &[&syn::Lifetime]) -> bool {
    !used_lifetimes_in_type(ty, lifetime_params).is_empty()
}

/// Find all lifetimes from the given set that are used in type param bounds
pub(crate) fn used_lifetimes_in_bounds<'a>(
    bounds: &syn::punctuated::Punctuated<syn::TypeParamBound, syn::token::Plus>,
    lifetime_params: &'a [&'a syn::Lifetime],
) -> BTreeSet<syn::Lifetime> {
    let mut visitor = LifetimeVisitor::new(lifetime_params);
    for bound in bounds {
        syn::visit::Visit::visit_type_param_bound(&mut visitor, bound);
    }
    visitor.into_found()
}

pub(crate) fn used_generic_params<'a>(
    ty: &'a syn::Type,
    generic_names: &'a Vec<&Ident>,
    mut used_params: BTreeSet<Ident>,
) -> BTreeSet<Ident> {
    let mut visitor = GenericNameVisitor::new(generic_names, &mut used_params);
    visitor.visit_type(ty);
    used_params
}

/// Usage visitor for generic bounds
pub(crate) fn generics_predicate_uses(
    predicate: &syn::WherePredicate,
    generic_names: &Vec<&Ident>,
) -> bool {
    let mut found_set = Default::default();
    let mut visitor = GenericNameVisitor::new(generic_names, &mut found_set);
    visitor.visit_where_predicate(predicate);
    !found_set.is_empty()
}

/// Concrete type replacement visitor application.
/// Replaces generic type parameters with their concrete types (or JsValue if no default),
/// and replaces specified lifetime parameters with 'static (since extern blocks cannot have
/// lifetime parameters from the outer scope).
pub(crate) fn generic_to_concrete<'a>(
    mut ty: syn::Type,
    generic_names: &BTreeMap<&'a Ident, Option<Cow<'a, syn::Type>>>,
    lifetimes_to_staticize: &[&syn::Lifetime],
) -> Result<syn::Type, Diagnostic> {
    // First, replace type parameters with their concrete types
    if !generic_names.is_empty() {
        let mut visitor = GenericRenameVisitor {
            renames: generic_names,
            err: None,
        };
        visitor.visit_type_mut(&mut ty);
        if let Some(err) = visitor.err {
            return Err(err);
        }
    }
    // Then, replace specified lifetimes with 'static for ABI compatibility
    Ok(staticize_lifetimes(ty, lifetimes_to_staticize))
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_generic_name_visitor() {
        let t_ident = syn::Ident::new("T", proc_macro2::Span::call_site());
        let u_ident = syn::Ident::new("U", proc_macro2::Span::call_site());
        let generic_params = vec![&t_ident, &u_ident];

        // Test T as value
        let ty: syn::Type = syn::parse_quote!(T);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));

        // Test &T as reference
        let ty: syn::Type = syn::parse_quote!(&T);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));

        // Test T<U> - both found
        let ty: syn::Type = syn::parse_quote!(T<U>);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));
        assert!(visitor.found_set.contains(&u_ident));

        // Test &T<U> - both found
        let ty: syn::Type = syn::parse_quote!(&T<U>);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));
        assert!(visitor.found_set.contains(&u_ident));

        // Test T::<U>::Foo - T and U found, Foo ignored
        let ty: syn::Type = syn::parse_quote!(T::<U>::Foo);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));
        assert!(visitor.found_set.contains(&u_ident));

        // Test Vec<T> - T found, Vec ignored
        let ty: syn::Type = syn::parse_quote!(Vec<T>);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));
        assert!(!visitor.found_set.contains(&u_ident));
    }

    #[test]
    fn test_associated_type_binding() {
        let t_ident = syn::Ident::new("T", proc_macro2::Span::call_site());
        let u_ident = syn::Ident::new("U", proc_macro2::Span::call_site());
        let generic_params = vec![&t_ident, &u_ident];

        // Test SomeTrait<T = U> - should find U (RHS) but NOT T (LHS assoc type name)
        let ty: syn::Type = syn::parse_quote!(SomeTrait<T = U>);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(!visitor.found_set.contains(&t_ident)); // T is LHS assoc type name
        assert!(visitor.found_set.contains(&u_ident)); // U is RHS generic parameter

        // Test SomeTrait<U = T> - should find T (RHS) but NOT U (LHS assoc type name)
        let ty: syn::Type = syn::parse_quote!(SomeTrait<U = T>);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident)); // T is RHS generic parameter
        assert!(!visitor.found_set.contains(&u_ident)); // U is LHS assoc type name
    }

    #[test]
    fn test_nested_references() {
        let t_ident = syn::Ident::new("T", proc_macro2::Span::call_site());
        let u_ident = syn::Ident::new("U", proc_macro2::Span::call_site());
        let generic_params = vec![&t_ident, &u_ident];

        // Test &T
        let ty: syn::Type = syn::parse_quote!(&T);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));

        // Test &&T
        let ty: syn::Type = syn::parse_quote!(&&T);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));

        // Test &&&T
        let ty: syn::Type = syn::parse_quote!(&&&T);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));

        // Test &T<U>
        let ty: syn::Type = syn::parse_quote!(&T<U>);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));
        assert!(visitor.found_set.contains(&u_ident));
    }

    #[test]
    fn test_mixed_usage() {
        let t_ident = syn::Ident::new("T", proc_macro2::Span::call_site());
        let generic_params = vec![&t_ident];

        // Test T appearing in multiple places
        let ty: syn::Type = syn::parse_quote!(SomeTrait<Item = T> + OtherTrait<Ref = &T>);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(visitor.found_set.contains(&t_ident));
    }

    #[test]
    fn test_ref_qself_trait_assoc_type() {
        let t_ident = syn::Ident::new("T", proc_macro2::Span::call_site());
        let generic_params = vec![&t_ident];

        // Test &<T as JsFunction1>::Arg1 - T should be found
        let ty: syn::Type = syn::parse_quote!(&<T as JsFunction1>::Arg1);
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);
        assert!(
            visitor.found_set.contains(&t_ident),
            "T should be found in &<T as JsFunction1>::Arg1"
        );
    }

    #[test]
    fn test_complex_reference_with_closure() {
        let t_ident = syn::Ident::new("T", proc_macro2::Span::call_site());
        let r_ident = syn::Ident::new("R", proc_macro2::Span::call_site());
        let generic_params = vec![&t_ident, &r_ident];

        let ty: syn::Type = syn::parse_quote!(&Closure<dyn FnMut(T) -> Result<R, JsValue>>);

        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_type(&mut visitor, &ty);

        assert!(visitor.found_set.contains(&t_ident));
        assert!(visitor.found_set.contains(&r_ident));
    }

    #[test]
    fn test_generic_args_to_concrete() {
        use std::borrow::Cow;
        use std::collections::BTreeMap;

        // T -> String replacement
        let t = syn::parse_quote!(T);
        let str: syn::Type = syn::parse_quote!(String);
        let generic_names: BTreeMap<&syn::Ident, Option<Cow<syn::Type>>> = {
            let mut map = BTreeMap::new();
            map.insert(&t, Some(Cow::Borrowed(&str)));
            map
        };

        // T gets replaced with String
        let generic_type: syn::Type = syn::parse_quote!(Promise<T>);
        let result =
            crate::generics::generic_to_concrete(generic_type, &generic_names, &[]).unwrap();
        let expected: syn::Type = syn::parse_quote!(Promise<String>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // Mixed: i32 stays, T becomes String
        let mixed_type: syn::Type = syn::parse_quote!(Promise<i32, T>);
        let result = crate::generics::generic_to_concrete(mixed_type, &generic_names, &[]).unwrap();
        let expected: syn::Type = syn::parse_quote!(Promise<i32, String>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // No generics to replace - unchanged
        let concrete_type: syn::Type = syn::parse_quote!(Promise<i32, bool>);
        let result =
            crate::generics::generic_to_concrete(concrete_type, &generic_names, &[]).unwrap();
        let expected: syn::Type = syn::parse_quote!(Promise<i32, bool>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );
    }

    #[test]
    fn test_generic_associated_type_replacement() {
        use std::borrow::Cow;
        use std::collections::BTreeMap;

        let t: syn::Ident = syn::parse_quote!(T);
        let concrete: syn::Type = syn::parse_quote!(MyConcreteType);
        let generic_names: BTreeMap<&syn::Ident, Option<Cow<syn::Type>>> = {
            let mut map = BTreeMap::new();
            map.insert(&t, Some(Cow::Borrowed(&concrete)));
            map
        };

        // T::DurableObjectStub -> MyConcreteType::DurableObjectStub
        let assoc_type: syn::Type = syn::parse_quote!(T::DurableObjectStub);
        let result = crate::generics::generic_to_concrete(assoc_type, &generic_names, &[]).unwrap();
        let expected: syn::Type = syn::parse_quote!(MyConcreteType::DurableObjectStub);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // Nested: Vec<T::Item> -> Vec<MyConcreteType::Item>
        let nested: syn::Type = syn::parse_quote!(Vec<T::Item>);
        let result = crate::generics::generic_to_concrete(nested, &generic_names, &[]).unwrap();
        let expected: syn::Type = syn::parse_quote!(Vec<MyConcreteType::Item>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // Complex: WasmRet<<T::Stub as FromWasmAbi>::Abi>
        let complex: syn::Type = syn::parse_quote!(WasmRet<<T::Stub as FromWasmAbi>::Abi>);
        let result = crate::generics::generic_to_concrete(complex, &generic_names, &[]).unwrap();
        let expected: syn::Type =
            syn::parse_quote!(WasmRet<<MyConcreteType::Stub as FromWasmAbi>::Abi>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // T<Foo> gets fully replaced, args discarded
        let with_args: syn::Type = syn::parse_quote!(T<SomeArg>);
        let result = crate::generics::generic_to_concrete(with_args, &generic_names, &[]).unwrap();
        let expected: syn::Type = syn::parse_quote!(MyConcreteType);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // QSelf: <T::DurableObjectStub as FromWasmAbi>::Abi
        let qself_type: syn::Type = syn::parse_quote!(<T::DurableObjectStub as FromWasmAbi>::Abi);
        let result = crate::generics::generic_to_concrete(qself_type, &generic_names, &[]).unwrap();
        let expected: syn::Type =
            syn::parse_quote!(<MyConcreteType::DurableObjectStub as FromWasmAbi>::Abi);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // QSelf with trait: <T as DurableObject>::DurableObjectStub
        let qself_trait: syn::Type = syn::parse_quote!(<T as DurableObject>::DurableObjectStub);
        let result =
            crate::generics::generic_to_concrete(qself_trait, &generic_names, &[]).unwrap();
        let expected: syn::Type =
            syn::parse_quote!(<MyConcreteType as DurableObject>::DurableObjectStub);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // Reference to QSelf with trait: &<T as DurableObject>::DurableObjectStub
        let ref_qself_trait: syn::Type =
            syn::parse_quote!(&<T as DurableObject>::DurableObjectStub);
        let result =
            crate::generics::generic_to_concrete(ref_qself_trait, &generic_names, &[]).unwrap();
        let expected: syn::Type =
            syn::parse_quote!(&<MyConcreteType as DurableObject>::DurableObjectStub);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );
    }

    #[test]
    fn test_where_predicate_assoc_type_binding() {
        // Test that generics_predicate_uses finds generic params in associated type bindings
        // This is the pattern: F: JsFunction<Ret = Ret>
        // Both F and Ret should be detected as used

        let f_ident = syn::Ident::new("F", proc_macro2::Span::call_site());
        let ret_ident = syn::Ident::new("Ret", proc_macro2::Span::call_site());

        // Test with both F and Ret in the search set
        let generic_params = vec![&f_ident, &ret_ident];
        let predicate: syn::WherePredicate = syn::parse_quote!(F: JsFunction<Ret = Ret>);

        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_where_predicate(&mut visitor, &predicate);

        assert!(
            found_set.contains(&f_ident),
            "F should be found in 'F: JsFunction<Ret = Ret>'"
        );
        assert!(
            found_set.contains(&ret_ident),
            "Ret should be found in 'F: JsFunction<Ret = Ret>' (RHS of assoc type binding)"
        );
    }

    #[test]
    fn test_where_predicate_assoc_type_binding_only_rhs() {
        let f_ident = syn::Ident::new("F", proc_macro2::Span::call_site());
        let ret_ident = syn::Ident::new("Ret", proc_macro2::Span::call_site());

        // Ret in the search set
        let generic_params = vec![&ret_ident];
        let predicate: syn::WherePredicate = syn::parse_quote!(F: JsFunction<Ret = Ret>);

        let uses = crate::generics::generics_predicate_uses(&predicate, &generic_params);
        assert!(
            uses,
            "Ret should be detected as used in 'F: JsFunction<Ret = Ret>'"
        );

        // F in the search set
        let not_generic_params = vec![&f_ident];
        let uses = crate::generics::generics_predicate_uses(&predicate, &not_generic_params);
        assert!(
            uses,
            "F should not be detected as used in 'F: JsFunction<Ret = Ret>'"
        );
    }

    #[test]
    fn test_where_predicate_assoc_type_binding_only_bounded() {
        // Test that only F (not Ret) is found when Ret is not in the search set
        let f_ident = syn::Ident::new("F", proc_macro2::Span::call_site());
        let ret_ident = syn::Ident::new("Ret", proc_macro2::Span::call_site());

        // Only F in the search set
        let generic_params = vec![&f_ident];
        let predicate: syn::WherePredicate = syn::parse_quote!(F: JsFunction<Ret = Ret>);

        let uses = crate::generics::generics_predicate_uses(&predicate, &generic_params);
        assert!(
            uses,
            "F should be detected as used in 'F: JsFunction<Ret = Ret>'"
        );

        // Also verify Ret is NOT found when not in the search set
        let mut found_set = Default::default();
        let mut visitor = crate::generics::GenericNameVisitor::new(&generic_params, &mut found_set);
        syn::visit::visit_where_predicate(&mut visitor, &predicate);

        assert!(found_set.contains(&f_ident), "F should be found");
        assert!(
            !found_set.contains(&ret_ident),
            "Ret should NOT be found when not in search set"
        );
    }

    #[test]
    fn test_staticize_specific_lifetimes() {
        // Test that specified lifetimes in types are replaced with 'static
        let lifetime_a: syn::Lifetime = syn::parse_quote!('a);
        let lifetimes = [&lifetime_a];

        let ty: syn::Type = syn::parse_quote!(ImmediateClosure<'a, dyn FnMut(T) -> R>);
        let result = crate::generics::staticize_lifetimes(ty, &lifetimes);
        let expected: syn::Type = syn::parse_quote!(ImmediateClosure<'static, dyn FnMut(T) -> R>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // Test multiple lifetimes - only staticize specified ones
        let lifetime_b: syn::Lifetime = syn::parse_quote!('b);
        let lifetimes_both = [&lifetime_a, &lifetime_b];
        let ty: syn::Type = syn::parse_quote!(&'a SomeType<'b, T>);
        let result = crate::generics::staticize_lifetimes(ty, &lifetimes_both);
        let expected: syn::Type = syn::parse_quote!(&'static SomeType<'static, T>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // Test selective staticization - only 'a, not 'b
        let ty: syn::Type = syn::parse_quote!(&'a SomeType<'b, T>);
        let result = crate::generics::staticize_lifetimes(ty, &[&lifetime_a]);
        let expected: syn::Type = syn::parse_quote!(&'static SomeType<'b, T>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // Test no lifetimes to staticize (should be unchanged)
        let ty: syn::Type = syn::parse_quote!(Vec<T>);
        let result = crate::generics::staticize_lifetimes(ty, &[]);
        let expected: syn::Type = syn::parse_quote!(Vec<T>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );
    }

    #[test]
    fn test_generic_to_concrete_with_lifetimes() {
        use std::borrow::Cow;
        use std::collections::BTreeMap;

        // Test that generic_to_concrete replaces both type params AND specified lifetimes
        let t: syn::Ident = syn::parse_quote!(T);
        let concrete: syn::Type = syn::parse_quote!(JsValue);
        let generic_names: BTreeMap<&syn::Ident, Option<Cow<syn::Type>>> = {
            let mut map = BTreeMap::new();
            map.insert(&t, Some(Cow::Borrowed(&concrete)));
            map
        };

        // Create the lifetime 'a that we want to staticize
        let lifetime_a: syn::Lifetime = syn::parse_quote!('a);
        let lifetimes_to_staticize = [&lifetime_a];

        // ImmediateClosure<'a, dyn FnMut(T)> -> ImmediateClosure<'static, dyn FnMut(JsValue)>
        let ty: syn::Type = syn::parse_quote!(ImmediateClosure<'a, dyn FnMut(T)>);
        let result =
            crate::generics::generic_to_concrete(ty, &generic_names, &lifetimes_to_staticize)
                .unwrap();
        let expected: syn::Type = syn::parse_quote!(ImmediateClosure<'static, dyn FnMut(JsValue)>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );

        // Test that lifetimes NOT in the list are preserved
        let _lifetime_b: syn::Lifetime = syn::parse_quote!('b);
        let lifetimes_only_a = [&lifetime_a];
        let ty: syn::Type = syn::parse_quote!(Foo<'a, 'b>);
        let result =
            crate::generics::generic_to_concrete(ty, &BTreeMap::new(), &lifetimes_only_a).unwrap();
        let expected: syn::Type = syn::parse_quote!(Foo<'static, 'b>);
        assert_eq!(
            quote::quote!(#result).to_string(),
            quote::quote!(#expected).to_string()
        );
    }
}
