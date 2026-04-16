// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Visitor for determining whether a type has type and non-static lifetime parameters
//! (except for type macros, which are largely ignored).
//!
//! Note that poor handling of type macros in `derive(Yokeable)` can only result in
//! compiler errors at worst, not unsoundness.

use std::collections::HashSet;
use syn::ext::IdentExt as _;
use syn::visit::{
    visit_bound_lifetimes, visit_generic_param, visit_lifetime, visit_type, visit_type_path,
    visit_where_clause, Visit,
};
use syn::{GenericParam, Ident, Lifetime, Type, TypePath, WhereClause};

use super::lifetimes::ignored_lifetime_ident;

struct Visitor<'a> {
    /// The lifetime parameter of the yokeable, stripped of any leading `r#`
    lt_param: &'a Ident,
    /// Whether we found a usage of the `lt_param` lifetime (or its `raw` form)
    found_lt_param_usage: bool,
    /// The type parameters in scope, stripped of any leading `r#`s
    typarams: &'a HashSet<Ident>,
    /// Whether we found a type parameter (or its `raw` form)
    found_typaram_usage: bool,
    /// How many underscores should be added before "yoke" in the `'__[underscores]__yoke`
    /// lifetime used by the derive.
    ///
    /// This is one more than the maximum number of underscores in
    /// (possibly raw) `'__[underscores]__yoke` lifetimes bound by `for<>` binders,
    /// or 0 if no such bound lifetimes were found.
    min_underscores_for_yoke_lt: usize,
}

impl<'ast> Visit<'ast> for Visitor<'_> {
    fn visit_lifetime(&mut self, lt: &'ast Lifetime) {
        if lt.ident.unraw() == *self.lt_param {
            // Note that `for<>` binders cannot introduce a lifetime already in scope,
            // so `lt.ident` necessarily refers to the lifetime parameter of the yokeable.
            self.found_lt_param_usage = true;
        }
        visit_lifetime(self, lt)
    }

    fn visit_type_path(&mut self, ty: &'ast TypePath) {
        // We only need to check ty.path.get_ident() and not ty.qself or any
        // generics in ty.path because the visitor will eventually visit those
        // types on its own
        if let Some(ident) = ty.path.get_ident() {
            if self.typarams.contains(&ident.unraw()) {
                self.found_typaram_usage = true;
            }
        }
        visit_type_path(self, ty)
    }

    fn visit_bound_lifetimes(&mut self, lts: &'ast syn::BoundLifetimes) {
        for lt in &lts.lifetimes {
            if let GenericParam::Lifetime(lt) = lt {
                let maybe_underscores_yoke = lt.lifetime.ident.unraw().to_string();

                // Check if the unraw'd lifetime ident is `__[underscores]__yoke`
                if let Some(underscores) = maybe_underscores_yoke.strip_suffix("yoke") {
                    if underscores.as_bytes().iter().all(|byte| *byte == b'_') {
                        // Since `_` is ASCII, `underscores` consists entirely of `_` characters
                        // iff it consists entirely of `b'_'` bytes, which holds iff
                        // `underscores.len()` is the number of underscores.
                        self.min_underscores_for_yoke_lt = self.min_underscores_for_yoke_lt.max(
                            // 1 more underscore, so as not to conflict with this bound lt.
                            underscores.len() + 1,
                        );
                    }
                }
            }
        }
        visit_bound_lifetimes(self, lts);
    }
    // Type macros are ignored/skipped by default.
}

#[derive(Debug, PartialEq, Eq)]
pub struct CheckResult {
    /// Whether the checked type uses the given `lt_param`
    /// (possibly in its raw form).
    pub uses_lifetime_param: bool,
    /// Whether the checked type uses one of the type parameters in scope
    /// (possibly in its raw form).
    pub uses_type_params: bool,
    /// How many underscores should be added before "yoke" in the `'__[underscores]__yoke`
    /// lifetime used by the derive.
    ///
    /// This is one more than the maximum number of underscores in
    /// (possibly raw) `'__[underscores]__yoke` lifetimes bound by `for<>` binders,
    /// or 0 if no such bound lifetimes were found.
    pub min_underscores_for_yoke_lt: usize,
}

/// Checks if a type uses the yokeable type's lifetime parameter or type parameters,
/// given the local context of named type parameters and the lifetime parameter.
///
/// Crucially, the idents in `lt_param` and `typarams` are required to not have leading `r#`s.
///
/// Usage of const generic parameters is not checked.
pub fn check_type_for_parameters(
    lt_param: &Ident,
    typarams: &HashSet<Ident>,
    ty: &Type,
) -> CheckResult {
    let mut visit = Visitor {
        lt_param,
        found_lt_param_usage: false,
        typarams,
        found_typaram_usage: false,
        min_underscores_for_yoke_lt: 0,
    };
    visit_type(&mut visit, ty);

    CheckResult {
        uses_lifetime_param: visit.found_lt_param_usage,
        uses_type_params: visit.found_typaram_usage,
        min_underscores_for_yoke_lt: visit.min_underscores_for_yoke_lt,
    }
}

/// Check a generic parameter of a yokeable type for usage of lifetimes like `'yoke` or `'_yoke`
/// bound in for-binders.
///
/// Returns [`min_underscores_for_yoke_lt`](CheckResult::min_underscores_for_yoke_lt).
pub fn check_parameter_for_bound_lts(param: &GenericParam) -> usize {
    // Note that `lt_param` does not impact `min_underscores_for_yoke_lt`.
    let mut visit = Visitor {
        lt_param: &ignored_lifetime_ident(),
        found_lt_param_usage: false,
        typarams: &HashSet::new(),
        found_typaram_usage: false,
        min_underscores_for_yoke_lt: 0,
    };

    visit_generic_param(&mut visit, param);

    visit.min_underscores_for_yoke_lt
}

/// Check a where-clause for usage of lifetimes like `'yoke` or `'_yoke` bound in for-binders.
///
/// Returns [`min_underscores_for_yoke_lt`](CheckResult::min_underscores_for_yoke_lt).
pub fn check_where_clause_for_bound_lts(where_clause: &WhereClause) -> usize {
    // Note that `lt_param` does not impact `min_underscores_for_yoke_lt`.
    let mut visit = Visitor {
        lt_param: &ignored_lifetime_ident(),
        found_lt_param_usage: false,
        typarams: &HashSet::new(),
        found_typaram_usage: false,
        min_underscores_for_yoke_lt: 0,
    };

    visit_where_clause(&mut visit, where_clause);

    visit.min_underscores_for_yoke_lt
}

#[cfg(test)]
mod tests {
    use proc_macro2::Span;
    use std::collections::HashSet;
    use syn::{parse_quote, Ident};

    use super::{check_type_for_parameters, CheckResult};

    fn a_ident() -> Ident {
        Ident::new("a", Span::call_site())
    }

    fn yoke_ident() -> Ident {
        Ident::new("yoke", Span::call_site())
    }

    fn make_typarams(params: &[&str]) -> HashSet<Ident> {
        params
            .iter()
            .map(|x| Ident::new(x, Span::call_site()))
            .collect()
    }

    fn uses(lifetime: bool, typarams: bool) -> CheckResult {
        CheckResult {
            uses_lifetime_param: lifetime,
            uses_type_params: typarams,
            min_underscores_for_yoke_lt: 0,
        }
    }

    #[test]
    fn test_simple_type() {
        let environment = make_typarams(&["T", "U", "V"]);

        let ty = parse_quote!(Foo<'a, T>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, true));

        let ty = parse_quote!(Foo<T>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, true));

        let ty = parse_quote!(Foo<'static, T>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, true));

        let ty = parse_quote!(Foo<'a>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, false));

        let ty = parse_quote!(Foo<'a, Bar<U>, Baz<(V, u8)>>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, true));

        let ty = parse_quote!(Foo<'a, W>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, false));
    }

    #[test]
    fn test_assoc_types() {
        let environment = make_typarams(&["T"]);

        let ty = parse_quote!(<Foo as SomeTrait<'a, T>>::Output);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, true));

        let ty = parse_quote!(<Foo as SomeTrait<'static, T>>::Output);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, true));

        let ty = parse_quote!(<T as SomeTrait<'static, Foo>>::Output);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, true));
    }

    #[test]
    fn test_macro_types() {
        let environment = make_typarams(&["T"]);

        // Macro types are opaque. Note that the environment doesn't contain `U` or `V`,
        // and the `T` is basically ignored.

        let ty = parse_quote!(foo!(Foo<'a, T>));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, false));

        let ty = parse_quote!(foo!(Foo<T>));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, false));

        let ty = parse_quote!(foo!(Foo<'static, T>));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, false));

        let ty = parse_quote!(foo!(Foo<'a>));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, false));

        let ty = parse_quote!(foo!(Foo<'a, Bar<U>, Baz<(V, u8)>>));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, false));

        let ty = parse_quote!(foo!(Foo<'a, W>));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, false));
    }

    #[test]
    fn test_raw_types() {
        let environment = make_typarams(&["T", "U", "V"]);

        let ty = parse_quote!(Foo<'a, r#T>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, true));

        let ty = parse_quote!(Foo<r#T>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, true));

        let ty = parse_quote!(Foo<'static, r#T>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(false, true));

        let ty = parse_quote!(Foo<'a>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, false));

        let ty = parse_quote!(Foo<'a, Bar<r#U>, Baz<(r#V, u8)>>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, true));

        let ty = parse_quote!(Foo<'a, r#W>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check, uses(true, false));
    }

    #[test]
    fn test_yoke_lifetime() {
        let environment = make_typarams(&["T", "U", "V"]);

        let ty = parse_quote!(Foo<'yoke, r#T>);
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check.min_underscores_for_yoke_lt, 0);

        let ty = parse_quote!(for<'yoke> fn(&'yoke ()));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check.min_underscores_for_yoke_lt, 1);

        let ty = parse_quote!(for<'_yoke> fn(&'_yoke ()));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check.min_underscores_for_yoke_lt, 2);

        let ty = parse_quote!(for<'_yoke_> fn(&'_yoke_ ()));
        let check = check_type_for_parameters(&yoke_ident(), &environment, &ty);
        assert_eq!(check.min_underscores_for_yoke_lt, 0);

        let ty = parse_quote!(for<'_yoke, '___yoke> fn(&'_yoke (), &'___yoke ()));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check.min_underscores_for_yoke_lt, 4);

        let ty = parse_quote!(for<'___yoke> fn(for<'_yoke> fn(&'_yoke (), &'___yoke ())));
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check.min_underscores_for_yoke_lt, 4);

        let ty = parse_quote! {
            for<'yoke> fn(for<'_yoke> fn(for<'b> fn(&'b (), &'_yoke (), &'yoke ())))
        };
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check.min_underscores_for_yoke_lt, 2);

        let ty = parse_quote! {
            for<'yoke> fn(for<'r#_yoke> fn(for<'b> fn(&'b (), &'_yoke (), &'yoke ())))
        };
        let check = check_type_for_parameters(&a_ident(), &environment, &ty);
        assert_eq!(check.min_underscores_for_yoke_lt, 2);
    }
}
