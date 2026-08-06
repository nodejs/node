// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
// #![cfg_attr(not(any(test, doc)), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
    )
)]
#![warn(missing_docs)]

//! Custom derives for `Yokeable` from the `yoke` crate.

mod lifetimes;
mod visitor;

use proc_macro::TokenStream;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::ext::IdentExt as _;
use syn::spanned::Spanned;
use syn::{parse_macro_input, parse_quote, DeriveInput, GenericParam, Ident, WherePredicate};
use synstructure::Structure;

use self::lifetimes::{custom_lt, ignored_lifetime_ident, replace_lifetime, static_lt};
use self::visitor::{
    check_parameter_for_bound_lts, check_type_for_parameters, check_where_clause_for_bound_lts,
    CheckResult,
};

/// Custom derive for `yoke::Yokeable`.
///
/// If your struct contains `zerovec::ZeroMap`, then the compiler will not
/// be able to guarantee the lifetime covariance due to the generic types on
/// the `ZeroMap` itself. You must add the following attribute in order for
/// the custom derive to work with `ZeroMap`.
///
/// ```rust,ignore
/// #[derive(Yokeable)]
/// #[yoke(prove_covariance_manually)]
/// ```
///
/// Beyond this case, if the derive fails to compile due to lifetime issues, it likely
/// means that the lifetime is not covariant and `Yokeable` is not safe to implement.
#[proc_macro_derive(Yokeable, attributes(yoke))]
pub fn yokeable_derive(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    TokenStream::from(yokeable_derive_impl(&input))
}

/// A small amount of metadata about a field of the yokeable type
struct FieldParamUsage {
    uses_lt: bool,
    uses_ty: bool,
}

impl From<CheckResult> for FieldParamUsage {
    fn from(value: CheckResult) -> Self {
        Self {
            uses_lt: value.uses_lifetime_param,
            uses_ty: value.uses_type_params,
        }
    }
}

fn yokeable_derive_impl(input: &DeriveInput) -> TokenStream2 {
    let name = &input.ident;
    let tybounds = input
        .generics
        .params
        .iter()
        .filter_map(|param| {
            match param {
                GenericParam::Lifetime(_) => None,
                GenericParam::Type(ty) => {
                    // Strip out param defaults, we don't need them in the impl
                    let mut ty = ty.clone();
                    ty.eq_token = None;
                    ty.default = None;
                    Some(GenericParam::Type(ty))
                }
                // TODO: support const-generics in a future PR
                // GenericParam::Const(const_param) => {
                //     // Strip out param defaults, we don't need them in the impl
                //     let mut const_param = const_param.clone();
                //     const_param.eq_token = None;
                //     const_param.default = None;
                //     Some(GenericParam::Const(const_param))
                // }
                GenericParam::Const(_) => None,
            }
        })
        .collect::<Vec<_>>();
    let typarams = tybounds
        .iter()
        .map(|param| match param {
            // We filtered out lifetime parameters
            GenericParam::Lifetime(_) => unreachable!(),
            GenericParam::Type(ty) => ty.ident.clone(),
            // TODO: support const-generics in a future PR
            // GenericParam::Const(const_param) => const_param.ident.clone(),
            GenericParam::Const(_) => unreachable!(),
        })
        .collect::<Vec<_>>();
    let wherebounds = input
        .generics
        .where_clause
        .iter()
        .flat_map(|wc| wc.predicates.iter())
        // If some future version of Rust adds more than just lifetime and type where-bound
        // predicates, we may want to match more predicates.
        .filter(|p| matches!(p, WherePredicate::Type(_)))
        .collect::<Vec<_>>();
    // We require all type parameters be 'static, otherwise
    // the Yokeable impl becomes really unwieldy to generate safely.
    let static_bounds: Vec<WherePredicate> = tybounds
        .iter()
        .filter_map(|param| {
            if let GenericParam::Type(ty) = param {
                let ty = &ty.ident;
                Some(parse_quote!(#ty: 'static))
            } else {
                None
            }
        })
        .collect();
    // Above idents are *not* `unraw`d, because they may be emitted by the derive
    // (so they might actually need to be raw).

    // Either the `unraw`d first lifetime parameter of the yokeable, or some ignored ident.
    // This parameter affects `uses_lifetime_param` values of `CheckResult`s and `uses_lt`
    // values of `FieldParamUsage`, but those values only impact the generated code if there
    // is at least one lifetime parameter; therefore, the random ident doesn't matter.
    let lt_param = input
        .generics
        .lifetimes()
        .next()
        .map_or_else(ignored_lifetime_ident, |lt| lt.lifetime.ident.unraw());
    let typarams_env = tybounds
        .iter()
        .filter_map(|param| {
            if let GenericParam::Type(ty) = param {
                Some(ty.ident.unraw())
            } else {
                None
            }
        })
        .collect();
    let mut underscores_for_lt = 0;

    // We need to do this analysis even before the case where there are zero lifetime parameters
    // in order to choose a `'__[underscores]__yoke` lifetime.
    // We need to check:
    // - trait bounds on generic type parameters
    // - default types for generic type parameters
    // - type bounds on const generic parameters
    // - where-bounds
    // - field types
    // Checking lifetime parameters and default values for const generic parameters isn't
    // particularly useful, but does no harm, so the simplest approach to knock out the first three
    // is to just check every parameter.
    for param in &input.generics.params {
        underscores_for_lt = underscores_for_lt.max(check_parameter_for_bound_lts(param));
    }
    if let Some(where_clause) = &input.generics.where_clause {
        underscores_for_lt = underscores_for_lt.max(check_where_clause_for_bound_lts(where_clause));
    }

    let structure = {
        let mut structure = Structure::new(input);
        structure.bind_with(|_| synstructure::BindStyle::Move);
        structure
    };

    // Information from `synstructure::Structure`, whose ordering of fields is deterministic.
    // Note that it's crucial that we don't filter out any variants or fields from the `Structure`.
    let mut field_info: Vec<FieldParamUsage> = Vec::new();
    // This code creating `field_info` should not be carelessly modified, else it could cause
    // a panic in a below `expect`.
    for variant_info in structure.variants() {
        for field_binding_info in variant_info.bindings() {
            let field = field_binding_info.ast();
            // Note: `lt_param` and everything in `typarams_env` were `unraw`d
            let check_result = check_type_for_parameters(&lt_param, &typarams_env, &field.ty);

            underscores_for_lt = underscores_for_lt.max(check_result.min_underscores_for_yoke_lt);
            field_info.push(check_result.into());
        }
    }
    let field_info = field_info;

    // All usages of the `check_*` functions are above this point,
    // in order to ensure that `yoke_lt` is correct.
    let (yoke_lt, bound_lt) = {
        let underscores = vec![b'_'; underscores_for_lt];
        #[expect(clippy::expect_used, reason = "invariant is ensured immediately above")]
        let underscores = core::str::from_utf8(&underscores).expect("_ is ASCII and thus UTF-8");
        (
            format!("'{underscores}yoke"),
            format!("'_{underscores}yoke"),
        )
    };
    // This is used where the `Yokeable<'a>` trait uses `'a` by default
    let yoke_lt = custom_lt(&yoke_lt);
    // This is used where the `Yokeable<'a>` trait uses `'b` by default
    let bound_lt = custom_lt(&bound_lt);

    let mut lts = input.generics.lifetimes();

    if lts.next().is_none() {
        // There are 0 lifetime parameters.

        return quote! {
            // This is safe because there are no lifetime parameters, and `type Output = Self`.
            unsafe impl<#yoke_lt, #(#tybounds),*> yoke::Yokeable<#yoke_lt>
            for #name<#(#typarams),*>
            where
                #(#static_bounds,)*
                #(#wherebounds,)*
                Self: Sized
            {
                type Output = Self;
                #[inline]
                fn transform(&self) -> &Self::Output {
                    self
                }
                #[inline]
                fn transform_owned(self) -> Self::Output {
                    self
                }
                #[inline]
                unsafe fn make(this: Self::Output) -> Self {
                    this
                }
                #[inline]
                fn transform_mut<F>(&#yoke_lt mut self, f: F)
                where
                    F: 'static + for<#bound_lt> FnOnce(&#bound_lt mut Self::Output) {
                    f(self)
                }
            }
        };
    };

    if lts.next().is_some() {
        // We already extracted one lifetime into `source_lt`, so this means there are
        // multiple lifetimes.
        return syn::Error::new(
            input.generics.span(),
            "derive(Yokeable) cannot have multiple lifetime parameters",
        )
        .to_compile_error();
    }

    let manual_covariance = input.attrs.iter().any(|a| {
        if a.path().is_ident("yoke") {
            if let Ok(i) = a.parse_args::<Ident>() {
                if i == "prove_covariance_manually" {
                    return true;
                }
            }
        }
        false
    });

    if !manual_covariance {
        // This is safe because as long as `transform()` compiles,
        // we can be sure that `'a` is a covariant lifetime on `Self`.
        // (Using `'a` as shorthand for `#yoke_lt`.)
        //
        // In particular, the operand of `&raw const` is not a location where implicit
        // type coercion can occur, so the type of `&raw const self` is `*const &'a Self`.
        // The RHS of a `let` with an explicit type annotation allows type coercion, so
        // `transform` checks that `*const &'a Self` can coerce to `*const &'a Self::Output`.
        // Most of the possible type coercions
        // (listed at https://doc.rust-lang.org/reference/type-coercions.html)
        // do not apply, other than subtyping coercions and transitive coercions (which do
        // not add anything beyond subtyping coercions). In particular, there's nothing
        // like a `DerefRaw` on `*const T`, and `&T` does not implement `Unsize`, so
        // there cannot be an unsizing coercion from `*const &'a Self` to
        // `*const &'a Self::Output`. Therefore, `transform` compiles if and only if
        // a subtyping coercion is possible; this requires that `Self` must be a subtype
        // of `Self::Output`, just as `&'static T` is a subtype of `&'a T` (for `T: 'static`).
        // This ensures covariance.
        //
        // This will not work for structs involving ZeroMap since
        // the compiler does not know that ZeroMap is covariant.
        //
        // This custom derive can be improved to handle this case when necessary,
        // with `prove_covariance_manually`.
        return quote! {
            unsafe impl<#yoke_lt, #(#tybounds),*> yoke::Yokeable<#yoke_lt>
            for #name<'static, #(#typarams),*>
            where
                #(#static_bounds,)*
                #(#wherebounds,)*
                // Adding `Self: Sized` here doesn't work.
                // `for<#bound_lt> #name<#bound_lt, #(#typarams),*>: Sized`
                // might work, though. Since these trait bounds are very finicky, it's best to just
                // not try unless necessary.
            {
                type Output = #name<#yoke_lt, #(#typarams),*>;
                #[inline]
                fn transform(&#yoke_lt self) -> &#yoke_lt Self::Output {
                    if false {
                        let _: *const &#yoke_lt Self::Output = &raw const self;
                    }
                    self
                }
                #[inline]
                fn transform_owned(self) -> Self::Output {
                    self
                }
                #[inline]
                unsafe fn make(from: Self::Output) -> Self {
                    ::core::mem::transmute::<Self::Output, Self>(from)
                }
                #[inline]
                fn transform_mut<F>(&#yoke_lt mut self, f: F)
                where
                    F: 'static + for<#bound_lt> FnOnce(&#bound_lt mut Self::Output) {
                    let y = unsafe { &mut *(self as *mut Self as *mut Self::Output) };
                    f(y)
                }
            }
        };
    }

    // `prove_covariance_manually` requires additional bounds
    let mut manual_proof_bounds: Vec<WherePredicate> = Vec::new();
    let mut yokeable_checks = TokenStream2::new();
    let mut output_checks = TokenStream2::new();
    let mut field_info = field_info.into_iter();

    // See `synstructure::Structure::each` and `synstructure::VariantInfo::each`
    // for the setup of the two `*_checks` token streams. We can elide some brackets compared
    // to `synstructure`, since we know that each check defines no local variables, items, etc.

    // We iterate over the fields of `structure` in the same way that `field_info` was created.
    for variant_info in structure.variants() {
        let mut yokeable_check_body = TokenStream2::new();
        let mut output_check_body = TokenStream2::new();

        for field_binding_info in variant_info.bindings() {
            let field = field_binding_info.ast();
            let field_binding = &field_binding_info.binding;

            // This invariant is somewhat complicated, but immutable variables, iteration order,
            // and creating/using one `FieldParamUsage per iteration ensure that `field_info`
            // has an entry for this field (and it refers to the expected field).
            #[expect(
                clippy::expect_used,
                reason = "See above comment; this should never panic"
            )]
            let FieldParamUsage { uses_lt, uses_ty } = field_info
                .next()
                .expect("fields of an unmutated synstructure::Structure should remain the same");

            // Note that this type could be a weird non-pure macro type. However, even though
            // we evaluate it once or twice in where-bounds, we evaluate it exactly once
            // in the soundness-critical checks, so it can't cause UB by unexpectedly evaluating
            // to a different type. That can only cause a compile error at worst.
            let fty_static = replace_lifetime(&lt_param, &field.ty, static_lt());

            // For field types that don't use type or lifetime parameters, we don't add `Yokeable`
            // or `'static` where-bounds, and the field is required to unconditionally meet a
            // `'static` requirement (in its output form).
            //
            // For field types that use the lifetime parameter but no type parameters, we also don't
            // add any where-bounds, and the field is required to unconditionally meet a
            // `Yokeable` requirement (in its static yokeable form).
            // (The compiler should be able to figure out whether that requirement is satisfied.
            // A where-bound is intentionally avoided, to avoid letting `derive(Yokeable)`
            // compile on a struct when it's statically known that the where-bound is never
            // satisfied.)
            //
            // For field types that use a type parameter but not the lifetime parameter, the field
            // is assumed not to borrow from the cart and is therefore required to be `'static`
            // (in its output form), and a where-bound is added for this field being `'static`.
            //
            // For field types that use both the lifetime parameter and type parameters, the
            // field is required to be `Yokeable` (in its static form). Since there may be complex
            // preconditions to `FieldTy: Yokeable` that need to be satisfied, a where-bound
            // requires that `FieldTy<'static>: Yokeable<#yoke_lt, Output = FieldTy<#yoke_lt>>`.
            // This requirement is also tested on the field's static yokeable form.

            // Note: if `field.ty` involves a non-pure macro type, each time it's evaluated, it
            // could be a different type. The where-bounds are relied on to make the impl compile
            // in sane cases, *not* for soundness. Our `transform()` impl does not blindly assume
            // that the fields' types implement `Yokeable` or `'static`, regardless of these bounds.
            if uses_ty {
                if uses_lt {
                    let fty_output = replace_lifetime(&lt_param, &field.ty, yoke_lt.clone());

                    manual_proof_bounds.push(
                        parse_quote!(#fty_static: yoke::Yokeable<#yoke_lt, Output = #fty_output>),
                    );
                } else {
                    manual_proof_bounds.push(parse_quote!(#fty_static: 'static));
                }
            }
            if uses_lt {
                // This confirms that this `FieldTy` is a subtype of something which implements
                // `Yokeable<'a>`, and since only `'static` types can be subtypes of a `'static`
                // type (and all `Yokeable` implementors are `'static`), we have that either:
                // - `FieldTy` is some `'static` type which does NOT implement `Yokeable`, but via
                //   function pointer subtyping or something similar, is a subtype of something
                //   implementing `Yokeable`, or
                // - `FieldTy` is some type which does itself implement `Yokeable`.
                // In either of those cases, it is sound to treat `FieldTy` as covariant in the `'a`
                // parameter. (Using `'a` as shorthand for `#yoke_lt`.)
                //
                // Now, to justify that `FieldTy` (the field's actual type,
                // not just `field.ty`, which may have a non-pure macro type)
                // is a subtype of something which implements `Yokeable<'a>`:
                //
                // `#field_binding` has type `&'a FieldTy` (since it's a field of `&'a Self` matched
                // as `self`). The operand of `&raw const` is not a location where implicit type
                // coercion can occur. Therefore, `&raw const #field_binding` is guaranteed to be
                // type `*const &'a FieldTy`. The argument to `__yoke_derive_require_yokeable`
                // does allow type coercion.
                // Looking at <https://doc.rust-lang.org/reference/type-coercions.html>,
                // there are only three types of coercions that could plausibly apply:
                // - subtyping coercions,
                // - transitive coercions, and
                // - unsizing coercions.
                // (If some sort of `DerefRaw` trait gets added for `*const`, there could plausibly
                // be problems with that. But there's no reason to think that such a trait will be
                // added, since it'd mess with `unsafe` code, and Rust devs should recognize that.)
                //
                // Since `&'a _` does not implement `Unsize`, we have that `*const &'a _` does not
                // allow an unsizing coercion to occur. Therefore, there are only subtyping
                // coercions, since transitive coercions add nothing on top of subtyping coercions.
                // Therefore, if this compiles, `*const &'a FieldTy` must be a subtype of
                // `*const &'a T` where `T = #fty_static` is the generic parameter of
                // `__yoke_derive_require_yokeable`.
                // Looking at the signature of that function generated below, we have that
                // `T: Yokeable<'a>` (if it compiles). Note that if `#fty_static` is incorrect,
                // even if there is some other `T` which would work, this will just fail to compile.
                // Since `*const _` and `&'a _` are covariant over their type parameters, we have
                // that `FieldTy` must be a subtype of `T` in order for a subtyping coercion from
                // `*const &'a FieldTy` to `*const &'a T` to occur.
                //
                // Therefore, `FieldTy` must be a subtype of something which implements
                // `Yokeable<'a>` in order for this to compile. (Though that is not a _sufficient_
                // condition to compile, as some weird macro type could break stuff.)
                yokeable_check_body.extend(quote! {
                    __yoke_derive_require_yokeable::<#yoke_lt, #fty_static>(&raw const #field_binding);
                });
            } else {
                // No visible nested lifetimes, so there should be nothing to be done in sane cases.
                // However, in case a macro type does something strange and accesses the available
                // `#yoke_lt` lifetime, we still need to check that the field's actual type is
                // `'static` regardless of `#yoke_lt` (which we can check by ensuring that it
                // be a subtype of a `'static` type).
                // See reasoning in the `if` branch for why this works. The difference is that
                // `FieldTy` is guaranteed to be a subtype of `T = #fty_static` where `T: 'static`
                // (if this compiles). Since the field's type is a subtype of something which is
                // `'static`, it must itself be `'static`, and therefore did not manage to use
                // `#yoke_lt` via a macro.
                // Note that creating and using `#fty_output` is not necessary here, since
                // `field.ty == fty_static == fty_output` (no lifetime was visibly present which
                // could be replaced).
                output_check_body.extend(quote! {
                    __yoke_derive_require_static::<#yoke_lt, #fty_static>(&raw const #field_binding);
                });
            }
        }

        let pat = variant_info.pat();
        yokeable_checks.extend(quote! { #pat => { #yokeable_check_body }});
        output_checks.extend(quote! { #pat => { #output_check_body }});
    }

    quote! {
        // SAFETY: we assert covariance in `borrowed_checks`
        unsafe impl<#yoke_lt, #(#tybounds),*> yoke::Yokeable<#yoke_lt>
        for #name<'static, #(#typarams),*>
        where
            #(#static_bounds,)*
            #(#wherebounds,)*
            #(#manual_proof_bounds,)*
            // Adding `Self: Sized` here doesn't work.
            // `for<#bound_lt> #name<#bound_lt, #(#typarams),*>: Sized`
            // might work, though. Since these trait bounds are very finicky, it's best to just
            // not try unless necessary.
        {
            type Output = #name<#yoke_lt, #(#typarams),*>;
            #[inline]
            fn transform(&#yoke_lt self) -> &#yoke_lt Self::Output {
                // These are just type asserts, we don't need to run them
                if false {
                    // This could, hypothetically, conflict with the name of one of the `FieldTy`s
                    // we read (and cause a compilation error). However, such a conflict cannot
                    // cause unsoundness, since this function is in scope no matter what.
                    // (The problem is that attempting to refer to a type named
                    // `__yoke_derive_require_yokeable` would instead refer to this function item
                    // and therefore fail.)
                    #[allow(dead_code)]
                    fn __yoke_derive_require_yokeable<
                        #yoke_lt: #yoke_lt,
                        T: yoke::Yokeable<#yoke_lt>,
                    >(_t: *const &#yoke_lt T) {}

                    match self {
                        #yokeable_checks
                    }
                }
                let output = unsafe { ::core::mem::transmute::<&#yoke_lt Self, &#yoke_lt Self::Output>(self) };
                if false {
                    // Same deal as above.
                    #[allow(dead_code)]
                    fn __yoke_derive_require_static<
                        #yoke_lt: #yoke_lt,
                        T: 'static,
                    >(_t: *const &#yoke_lt T) {}

                    match output {
                        #output_checks
                    }
                }
                output
            }
            #[inline]
            fn transform_owned(self) -> Self::Output {
                unsafe { ::core::mem::transmute::<Self, Self::Output>(self) }
            }
            #[inline]
            unsafe fn make(from: Self::Output) -> Self {
                unsafe { ::core::mem::transmute::<Self::Output, Self>(from) }
            }
            #[inline]
            fn transform_mut<F>(&#yoke_lt mut self, f: F)
            where
                F: 'static + for<#bound_lt> FnOnce(&#bound_lt mut Self::Output) {
                let y = unsafe { &mut *(self as *mut Self as *mut Self::Output) };
                f(y)
            }
        }
    }
}
