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
    unsafe impl ::zerocopy::IntoBytes for Foo
    where
        u8: ::zerocopy::IntoBytes,
        [Trailing]: ::zerocopy::IntoBytes,
        (): ::zerocopy::util::macro_util::DynamicPaddingFree<
            Self,
            {
                ::zerocopy::repr_c_struct_has_padding!(
                    Self,
                    (::zerocopy::util::macro_util::core_reexport::option::Option::None::
                    < ::zerocopy::util::macro_util::core_reexport::num::NonZeroUsize >),
                    (::zerocopy::util::macro_util::core_reexport::option::Option::None::
                    < ::zerocopy::util::macro_util::core_reexport::num::NonZeroUsize >),
                    [(u8), ([Trailing])]
                )
            },
        >,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
    }
};
