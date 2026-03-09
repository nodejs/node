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
    unsafe impl ::zerocopy::TryFromBytes for Foo
    where
        u8: ::zerocopy::TryFromBytes,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
        #[inline(always)]
        fn is_bit_valid<___ZcAlignment>(
            _candidate: ::zerocopy::Maybe<'_, Self, ___ZcAlignment>,
        ) -> ::zerocopy::util::macro_util::core_reexport::primitive::bool
        where
            ___ZcAlignment: ::zerocopy::invariant::Alignment,
        {
            if false {
                fn assert_is_from_bytes<T>()
                where
                    T: ::zerocopy::FromBytes,
                    T: ?::zerocopy::util::macro_util::core_reexport::marker::Sized,
                {}
                assert_is_from_bytes::<Self>();
            }
            true
        }
    }
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
        enum ẕa {}
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
            unsafe impl ::zerocopy::HasTag for Foo {
                fn only_derive_is_allowed_to_implement_this_trait() {}
                type Tag = ();
                type ProjectToTag = ::zerocopy::pointer::cast::CastToUnit;
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
            unsafe impl ::zerocopy::HasField<
                ẕa,
                { ::zerocopy::UNION_VARIANT_ID },
                { ::zerocopy::ident_id!(a) },
            > for Foo {
                fn only_derive_is_allowed_to_implement_this_trait() {}
                type Type = u8;
                #[inline(always)]
                fn project(
                    slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                ) -> *mut <Self as ::zerocopy::HasField<
                    ẕa,
                    { ::zerocopy::UNION_VARIANT_ID },
                    { ::zerocopy::ident_id!(a) },
                >>::Type {
                    let slf = slf.as_ptr();
                    unsafe {
                        ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                            (* slf).a
                        )
                    }
                }
            }
        };
    };
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
    unsafe impl ::zerocopy::FromZeros for Foo
    where
        u8: ::zerocopy::FromZeros,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
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
    unsafe impl ::zerocopy::FromBytes for Foo
    where
        u8: ::zerocopy::FromBytes,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
    }
};
