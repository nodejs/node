#[allow(
    deprecated,
    private_bounds,
    non_local_definitions,
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    non_ascii_idents,
    clippy::missing_inline_in_public_items,
)]
#[deny(ambiguous_associated_items)]
#[automatically_derived]
const _: () = {
    unsafe impl ::zerocopy::TryFromBytes for Foo {
        fn only_derive_is_allowed_to_implement_this_trait() {}
        #[inline]
        fn is_bit_valid<___ZcAlignment>(
            mut candidate: ::zerocopy::Maybe<'_, Self, ___ZcAlignment>,
        ) -> ::zerocopy::util::macro_util::core_reexport::primitive::bool
        where
            ___ZcAlignment: ::zerocopy::invariant::Alignment,
        {
            true
        }
    }
};
#[allow(
    deprecated,
    private_bounds,
    non_local_definitions,
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    non_ascii_idents,
    clippy::missing_inline_in_public_items,
)]
#[deny(ambiguous_associated_items)]
#[automatically_derived]
const _: () = {
    unsafe impl ::zerocopy::FromZeros for Foo {
        fn only_derive_is_allowed_to_implement_this_trait() {}
    }
};
