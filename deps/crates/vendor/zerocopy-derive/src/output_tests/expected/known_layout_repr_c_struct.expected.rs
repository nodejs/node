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
    unsafe impl<T, U> ::zerocopy::KnownLayout for Foo<T, U>
    where
        U: ::zerocopy::KnownLayout,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
        type PointerMetadata = <U as ::zerocopy::KnownLayout>::PointerMetadata;
        type MaybeUninit = __ZerocopyKnownLayoutMaybeUninit<T, U>;
        const LAYOUT: ::zerocopy::DstLayout = ::zerocopy::DstLayout::for_repr_c_struct(
            ::zerocopy::util::macro_util::core_reexport::num::NonZeroUsize::new(
                2u32 as usize,
            ),
            ::zerocopy::util::macro_util::core_reexport::option::Option::None,
            &[
                ::zerocopy::DstLayout::for_type::<T>(),
                <U as ::zerocopy::KnownLayout>::LAYOUT,
            ],
        );
        #[inline(always)]
        fn raw_from_ptr_len(
            bytes: ::zerocopy::util::macro_util::core_reexport::ptr::NonNull<u8>,
            meta: <Self as ::zerocopy::KnownLayout>::PointerMetadata,
        ) -> ::zerocopy::util::macro_util::core_reexport::ptr::NonNull<Self> {
            let trailing = <U as ::zerocopy::KnownLayout>::raw_from_ptr_len(bytes, meta);
            let slf = trailing.as_ptr() as *mut Self;
            unsafe {
                ::zerocopy::util::macro_util::core_reexport::ptr::NonNull::new_unchecked(
                    slf,
                )
            }
        }
        #[inline(always)]
        fn pointer_to_metadata(
            ptr: *mut Self,
        ) -> <Self as ::zerocopy::KnownLayout>::PointerMetadata {
            <U>::pointer_to_metadata(ptr as *mut _)
        }
    }
    struct __Zerocopy_Field_0;
    struct __Zerocopy_Field_1;
    unsafe impl<T, U> ::zerocopy::util::macro_util::Field<__Zerocopy_Field_0>
    for Foo<T, U> {
        type Type = T;
    }
    unsafe impl<T, U> ::zerocopy::util::macro_util::Field<__Zerocopy_Field_1>
    for Foo<T, U> {
        type Type = U;
    }
    #[repr(C)]
    #[repr(align(2))]
    #[doc(hidden)]
    struct __ZerocopyKnownLayoutMaybeUninit<T, U>(
        ::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
            <Foo<T, U> as ::zerocopy::util::macro_util::Field<__Zerocopy_Field_0>>::Type,
        >,
        ::zerocopy::util::macro_util::core_reexport::mem::ManuallyDrop<
            <<Foo<
                T,
                U,
            > as ::zerocopy::util::macro_util::Field<
                __Zerocopy_Field_1,
            >>::Type as ::zerocopy::KnownLayout>::MaybeUninit,
        >,
    )
    where
        <Foo<
            T,
            U,
        > as ::zerocopy::util::macro_util::Field<
            __Zerocopy_Field_1,
        >>::Type: ::zerocopy::KnownLayout;
    unsafe impl<T, U> ::zerocopy::KnownLayout for __ZerocopyKnownLayoutMaybeUninit<T, U>
    where
        <Foo<
            T,
            U,
        > as ::zerocopy::util::macro_util::Field<
            __Zerocopy_Field_1,
        >>::Type: ::zerocopy::KnownLayout,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
        type PointerMetadata = <Foo<T, U> as ::zerocopy::KnownLayout>::PointerMetadata;
        type MaybeUninit = Self;
        const LAYOUT: ::zerocopy::DstLayout = <Foo<
            T,
            U,
        > as ::zerocopy::KnownLayout>::LAYOUT;
        #[inline(always)]
        fn raw_from_ptr_len(
            bytes: ::zerocopy::util::macro_util::core_reexport::ptr::NonNull<u8>,
            meta: <Self as ::zerocopy::KnownLayout>::PointerMetadata,
        ) -> ::zerocopy::util::macro_util::core_reexport::ptr::NonNull<Self> {
            let trailing = <<<Foo<
                T,
                U,
            > as ::zerocopy::util::macro_util::Field<
                __Zerocopy_Field_1,
            >>::Type as ::zerocopy::KnownLayout>::MaybeUninit as ::zerocopy::KnownLayout>::raw_from_ptr_len(
                bytes,
                meta,
            );
            let slf = trailing.as_ptr() as *mut Self;
            unsafe {
                ::zerocopy::util::macro_util::core_reexport::ptr::NonNull::new_unchecked(
                    slf,
                )
            }
        }
        #[inline(always)]
        fn pointer_to_metadata(
            ptr: *mut Self,
        ) -> <Self as ::zerocopy::KnownLayout>::PointerMetadata {
            <<<Foo<
                T,
                U,
            > as ::zerocopy::util::macro_util::Field<
                __Zerocopy_Field_1,
            >>::Type as ::zerocopy::KnownLayout>::MaybeUninit>::pointer_to_metadata(
                ptr as *mut _,
            )
        }
    }
};
