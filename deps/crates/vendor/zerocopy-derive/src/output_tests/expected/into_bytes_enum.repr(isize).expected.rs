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
        (): ::zerocopy::util::macro_util::PaddingFree<
            Self,
            {
                #[repr(isize)]
                #[allow(dead_code)]
                pub enum ___ZerocopyTag {
                    Bar,
                }
                unsafe impl ::zerocopy::Immutable for ___ZerocopyTag {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                }
                ::zerocopy::enum_padding!(
                    Self,
                    (::zerocopy::util::macro_util::core_reexport::option::Option::None::
                    < ::zerocopy::util::macro_util::core_reexport::num::NonZeroUsize >),
                    (::zerocopy::util::macro_util::core_reexport::option::Option::None::
                    < ::zerocopy::util::macro_util::core_reexport::num::NonZeroUsize >),
                    ___ZerocopyTag, []
                )
            },
        >,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
    }
};
