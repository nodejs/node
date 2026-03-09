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
    unsafe impl<'a: 'static, X, Y: Deref, const N: usize> ::zerocopy::TryFromBytes
    for ComplexWithGenerics<'a, { N }, X, Y>
    where
        X: Deref<Target = &'a [(X, Y); N]>,
        u8: ::zerocopy::TryFromBytes,
        X: ::zerocopy::TryFromBytes,
        X::Target: ::zerocopy::TryFromBytes,
        Y::Target: ::zerocopy::TryFromBytes,
        [(X, Y); N]: ::zerocopy::TryFromBytes,
        bool: ::zerocopy::TryFromBytes,
        Y: ::zerocopy::TryFromBytes,
        PhantomData<&'a [(X, Y); N]>: ::zerocopy::TryFromBytes,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
        #[inline]
        fn is_bit_valid<___ZcAlignment>(
            mut candidate: ::zerocopy::Maybe<'_, Self, ___ZcAlignment>,
        ) -> ::zerocopy::util::macro_util::core_reexport::primitive::bool
        where
            ___ZcAlignment: ::zerocopy::invariant::Alignment,
        {
            #[repr(C)]
            #[allow(dead_code)]
            pub enum ___ZerocopyTag {
                UnitLike,
                StructLike,
                TupleLike,
            }
            unsafe impl ::zerocopy::Immutable for ___ZerocopyTag {
                fn only_derive_is_allowed_to_implement_this_trait() {}
            }
            type ___ZerocopyTagPrimitive = ::zerocopy::util::macro_util::SizeToTag<
                {
                    ::zerocopy::util::macro_util::core_reexport::mem::size_of::<
                        ___ZerocopyTag,
                    >()
                },
            >;
            const ___ZEROCOPY_TAG_UnitLike: ___ZerocopyTagPrimitive = ___ZerocopyTag::UnitLike
                as ___ZerocopyTagPrimitive;
            const ___ZEROCOPY_TAG_StructLike: ___ZerocopyTagPrimitive = ___ZerocopyTag::StructLike
                as ___ZerocopyTagPrimitive;
            const ___ZEROCOPY_TAG_TupleLike: ___ZerocopyTagPrimitive = ___ZerocopyTag::TupleLike
                as ___ZerocopyTagPrimitive;
            type ___ZerocopyOuterTag = ___ZerocopyTag;
            type ___ZerocopyInnerTag = ();
            #[repr(C)]
            struct ___ZerocopyVariantStruct_StructLike<
                'a: 'static,
                const N: usize,
                X,
                Y: Deref,
            >(
                ::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
                    ___ZerocopyInnerTag,
                >,
                u8,
                X,
                X::Target,
                Y::Target,
                [(X, Y); N],
                ::zerocopy::util::macro_util::core_reexport::marker::PhantomData<
                    ComplexWithGenerics<'a, N, X, Y>,
                >,
            )
            where
                X: Deref<Target = &'a [(X, Y); N]>;
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::TryFromBytes
                for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                    ::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
                        ___ZerocopyInnerTag,
                    >: ::zerocopy::TryFromBytes,
                    u8: ::zerocopy::TryFromBytes,
                    X: ::zerocopy::TryFromBytes,
                    X::Target: ::zerocopy::TryFromBytes,
                    Y::Target: ::zerocopy::TryFromBytes,
                    [(X, Y); N]: ::zerocopy::TryFromBytes,
                    ::zerocopy::util::macro_util::core_reexport::marker::PhantomData<
                        ComplexWithGenerics<'a, N, X, Y>,
                    >: ::zerocopy::TryFromBytes,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    #[inline]
                    fn is_bit_valid<___ZcAlignment>(
                        mut candidate: ::zerocopy::Maybe<'_, Self, ___ZcAlignment>,
                    ) -> ::zerocopy::util::macro_util::core_reexport::primitive::bool
                    where
                        ___ZcAlignment: ::zerocopy::invariant::Alignment,
                    {
                        true
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(0)
                                    } > ()
                                );
                                <::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
                                    ___ZerocopyInnerTag,
                                > as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(1)
                                    } > ()
                                );
                                <u8 as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(2)
                                    } > ()
                                );
                                <X as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(3)
                                    } > ()
                                );
                                <X::Target as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(4)
                                    } > ()
                                );
                                <Y::Target as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(5)
                                    } > ()
                                );
                                <[(
                                    X,
                                    Y,
                                ); N] as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(6)
                                    } > ()
                                );
                                <::zerocopy::util::macro_util::core_reexport::marker::PhantomData<
                                    ComplexWithGenerics<'a, N, X, Y>,
                                > as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
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
                    enum ẕ0 {}
                    enum ẕ1 {}
                    enum ẕ2 {}
                    enum ẕ3 {}
                    enum ẕ4 {}
                    enum ẕ5 {}
                    enum ẕ6 {}
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasTag
                        for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ0,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(0) },
                        > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = ::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
                                ___ZerocopyInnerTag,
                            >;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ0,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(0) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).0
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ0,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(0) },
                            > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ1,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(1) },
                        > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = u8;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ1,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(1) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).1
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ1,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(1) },
                            > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ2,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(2) },
                        > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = X;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ2,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(2) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).2
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ2,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(2) },
                            > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ3,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(3) },
                        > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = X::Target;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ3,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(3) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).3
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ3,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(3) },
                            > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ4,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(4) },
                        > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = Y::Target;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ4,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(4) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).4
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ4,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(4) },
                            > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ5,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(5) },
                        > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = [(X, Y); N];
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ5,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(5) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).5
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ5,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(5) },
                            > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ6,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(6) },
                        > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = ::zerocopy::util::macro_util::core_reexport::marker::PhantomData<
                                ComplexWithGenerics<'a, N, X, Y>,
                            >;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ6,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(6) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).6
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ6,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(6) },
                            > for ___ZerocopyVariantStruct_StructLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
                        };
                    };
                };
            };
            #[repr(C)]
            struct ___ZerocopyVariantStruct_TupleLike<
                'a: 'static,
                const N: usize,
                X,
                Y: Deref,
            >(
                ::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
                    ___ZerocopyInnerTag,
                >,
                bool,
                Y,
                PhantomData<&'a [(X, Y); N]>,
                ::zerocopy::util::macro_util::core_reexport::marker::PhantomData<
                    ComplexWithGenerics<'a, N, X, Y>,
                >,
            )
            where
                X: Deref<Target = &'a [(X, Y); N]>;
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::TryFromBytes
                for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                    ::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
                        ___ZerocopyInnerTag,
                    >: ::zerocopy::TryFromBytes,
                    bool: ::zerocopy::TryFromBytes,
                    Y: ::zerocopy::TryFromBytes,
                    PhantomData<&'a [(X, Y); N]>: ::zerocopy::TryFromBytes,
                    ::zerocopy::util::macro_util::core_reexport::marker::PhantomData<
                        ComplexWithGenerics<'a, N, X, Y>,
                    >: ::zerocopy::TryFromBytes,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    #[inline]
                    fn is_bit_valid<___ZcAlignment>(
                        mut candidate: ::zerocopy::Maybe<'_, Self, ___ZcAlignment>,
                    ) -> ::zerocopy::util::macro_util::core_reexport::primitive::bool
                    where
                        ___ZcAlignment: ::zerocopy::invariant::Alignment,
                    {
                        true
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(0)
                                    } > ()
                                );
                                <::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
                                    ___ZerocopyInnerTag,
                                > as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(1)
                                    } > ()
                                );
                                <bool as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(2)
                                    } > ()
                                );
                                <Y as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(3)
                                    } > ()
                                );
                                <PhantomData<
                                    &'a [(X, Y); N],
                                > as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
                            && {
                                let field_candidate = ::zerocopy::into_inner!(
                                    candidate.reborrow().project:: < _, {
                                    ::zerocopy::STRUCT_VARIANT_ID }, { ::zerocopy::ident_id!(4)
                                    } > ()
                                );
                                <::zerocopy::util::macro_util::core_reexport::marker::PhantomData<
                                    ComplexWithGenerics<'a, N, X, Y>,
                                > as ::zerocopy::TryFromBytes>::is_bit_valid(
                                    field_candidate,
                                )
                            }
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
                    enum ẕ0 {}
                    enum ẕ1 {}
                    enum ẕ2 {}
                    enum ẕ3 {}
                    enum ẕ4 {}
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasTag
                        for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ0,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(0) },
                        > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = ::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
                                ___ZerocopyInnerTag,
                            >;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ0,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(0) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).0
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ0,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(0) },
                            > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ1,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(1) },
                        > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = bool;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ1,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(1) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).1
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ1,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(1) },
                            > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ2,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(2) },
                        > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = Y;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ2,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(2) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).2
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ2,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(2) },
                            > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ3,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(3) },
                        > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = PhantomData<&'a [(X, Y); N]>;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ3,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(3) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).3
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ3,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(3) },
                            > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            const N: usize,
                        > ::zerocopy::HasField<
                            ẕ4,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(4) },
                        > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                        where
                            X: Deref<Target = &'a [(X, Y); N]>,
                        {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Type = ::zerocopy::util::macro_util::core_reexport::marker::PhantomData<
                                ComplexWithGenerics<'a, N, X, Y>,
                            >;
                            #[inline(always)]
                            fn project(
                                slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                            ) -> *mut <Self as ::zerocopy::HasField<
                                ẕ4,
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(4) },
                            >>::Type {
                                let slf = slf.as_ptr();
                                unsafe {
                                    ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                        (* slf).4
                                    )
                                }
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
                            unsafe impl<
                                'a: 'static,
                                X,
                                Y: Deref,
                                Aliasing: ::zerocopy::invariant::Aliasing,
                                Alignment: ::zerocopy::invariant::Alignment,
                                const N: usize,
                            > ::zerocopy::ProjectField<
                                ẕ4,
                                (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                                { ::zerocopy::STRUCT_VARIANT_ID },
                                { ::zerocopy::ident_id!(4) },
                            > for ___ZerocopyVariantStruct_TupleLike<'a, { N }, X, Y>
                            where
                                X: Deref<Target = &'a [(X, Y); N]>,
                            {
                                fn only_derive_is_allowed_to_implement_this_trait() {}
                                type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                                type Invariants = (
                                    Aliasing,
                                    Alignment,
                                    ::zerocopy::invariant::Initialized,
                                );
                            }
                        };
                    };
                };
            };
            #[repr(C)]
            union ___ZerocopyVariants<'a: 'static, const N: usize, X, Y: Deref> {
                __field_StructLike: ::zerocopy::util::macro_util::core_reexport::mem::ManuallyDrop<
                    ___ZerocopyVariantStruct_StructLike<'a, N, X, Y>,
                >,
                __field_TupleLike: ::zerocopy::util::macro_util::core_reexport::mem::ManuallyDrop<
                    ___ZerocopyVariantStruct_TupleLike<'a, N, X, Y>,
                >,
                __nonempty: (),
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
                enum ẕ__field_StructLike {}
                enum ẕ__field_TupleLike {}
                enum ẕ__nonempty {}
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
                    unsafe impl<
                        'a: 'static,
                        X,
                        Y: Deref,
                        const N: usize,
                    > ::zerocopy::HasTag for ___ZerocopyVariants<'a, { N }, X, Y> {
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
                    unsafe impl<
                        'a: 'static,
                        X,
                        Y: Deref,
                        const N: usize,
                    > ::zerocopy::HasField<
                        ẕ__field_StructLike,
                        { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                        { ::zerocopy::ident_id!(__field_StructLike) },
                    > for ___ZerocopyVariants<'a, { N }, X, Y> {
                        fn only_derive_is_allowed_to_implement_this_trait() {}
                        type Type = ::zerocopy::util::macro_util::core_reexport::mem::ManuallyDrop<
                            ___ZerocopyVariantStruct_StructLike<'a, N, X, Y>,
                        >;
                        #[inline(always)]
                        fn project(
                            slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                        ) -> *mut <Self as ::zerocopy::HasField<
                            ẕ__field_StructLike,
                            { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                            { ::zerocopy::ident_id!(__field_StructLike) },
                        >>::Type {
                            let slf = slf.as_ptr();
                            unsafe {
                                ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                    (* slf).__field_StructLike
                                )
                            }
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
                    unsafe impl<
                        'a: 'static,
                        X,
                        Y: Deref,
                        const N: usize,
                    > ::zerocopy::HasField<
                        ẕ__field_TupleLike,
                        { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                        { ::zerocopy::ident_id!(__field_TupleLike) },
                    > for ___ZerocopyVariants<'a, { N }, X, Y> {
                        fn only_derive_is_allowed_to_implement_this_trait() {}
                        type Type = ::zerocopy::util::macro_util::core_reexport::mem::ManuallyDrop<
                            ___ZerocopyVariantStruct_TupleLike<'a, N, X, Y>,
                        >;
                        #[inline(always)]
                        fn project(
                            slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                        ) -> *mut <Self as ::zerocopy::HasField<
                            ẕ__field_TupleLike,
                            { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                            { ::zerocopy::ident_id!(__field_TupleLike) },
                        >>::Type {
                            let slf = slf.as_ptr();
                            unsafe {
                                ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                    (* slf).__field_TupleLike
                                )
                            }
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
                    unsafe impl<
                        'a: 'static,
                        X,
                        Y: Deref,
                        const N: usize,
                    > ::zerocopy::HasField<
                        ẕ__nonempty,
                        { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                        { ::zerocopy::ident_id!(__nonempty) },
                    > for ___ZerocopyVariants<'a, { N }, X, Y> {
                        fn only_derive_is_allowed_to_implement_this_trait() {}
                        type Type = ();
                        #[inline(always)]
                        fn project(
                            slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                        ) -> *mut <Self as ::zerocopy::HasField<
                            ẕ__nonempty,
                            { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                            { ::zerocopy::ident_id!(__nonempty) },
                        >>::Type {
                            let slf = slf.as_ptr();
                            unsafe {
                                ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                    (* slf).__nonempty
                                )
                            }
                        }
                    }
                };
            };
            #[repr(C)]
            struct ___ZerocopyRawEnum<'a: 'static, const N: usize, X, Y: Deref> {
                tag: ___ZerocopyOuterTag,
                variants: ___ZerocopyVariants<'a, N, X, Y>,
            }
            unsafe impl<
                'a: 'static,
                const N: usize,
                X,
                Y: Deref,
            > ::zerocopy::pointer::InvariantsEq<___ZerocopyRawEnum<'a, N, X, Y>>
            for ComplexWithGenerics<'a, N, X, Y>
            where
                X: Deref<Target = &'a [(X, Y); N]>,
            {}
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
                enum ẕtag {}
                enum ẕvariants {}
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
                    unsafe impl<
                        'a: 'static,
                        X,
                        Y: Deref,
                        const N: usize,
                    > ::zerocopy::HasTag for ___ZerocopyRawEnum<'a, { N }, X, Y> {
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
                    unsafe impl<
                        'a: 'static,
                        X,
                        Y: Deref,
                        const N: usize,
                    > ::zerocopy::HasField<
                        ẕtag,
                        { ::zerocopy::STRUCT_VARIANT_ID },
                        { ::zerocopy::ident_id!(tag) },
                    > for ___ZerocopyRawEnum<'a, { N }, X, Y> {
                        fn only_derive_is_allowed_to_implement_this_trait() {}
                        type Type = ___ZerocopyOuterTag;
                        #[inline(always)]
                        fn project(
                            slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                        ) -> *mut <Self as ::zerocopy::HasField<
                            ẕtag,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(tag) },
                        >>::Type {
                            let slf = slf.as_ptr();
                            unsafe {
                                ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                    (* slf).tag
                                )
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            Aliasing: ::zerocopy::invariant::Aliasing,
                            Alignment: ::zerocopy::invariant::Alignment,
                            const N: usize,
                        > ::zerocopy::ProjectField<
                            ẕtag,
                            (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(tag) },
                        > for ___ZerocopyRawEnum<'a, { N }, X, Y> {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                            type Invariants = (
                                Aliasing,
                                Alignment,
                                ::zerocopy::invariant::Initialized,
                            );
                        }
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
                    unsafe impl<
                        'a: 'static,
                        X,
                        Y: Deref,
                        const N: usize,
                    > ::zerocopy::HasField<
                        ẕvariants,
                        { ::zerocopy::STRUCT_VARIANT_ID },
                        { ::zerocopy::ident_id!(variants) },
                    > for ___ZerocopyRawEnum<'a, { N }, X, Y> {
                        fn only_derive_is_allowed_to_implement_this_trait() {}
                        type Type = ___ZerocopyVariants<'a, N, X, Y>;
                        #[inline(always)]
                        fn project(
                            slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                        ) -> *mut <Self as ::zerocopy::HasField<
                            ẕvariants,
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(variants) },
                        >>::Type {
                            let slf = slf.as_ptr();
                            unsafe {
                                ::zerocopy::util::macro_util::core_reexport::ptr::addr_of_mut!(
                                    (* slf).variants
                                )
                            }
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
                        unsafe impl<
                            'a: 'static,
                            X,
                            Y: Deref,
                            Aliasing: ::zerocopy::invariant::Aliasing,
                            Alignment: ::zerocopy::invariant::Alignment,
                            const N: usize,
                        > ::zerocopy::ProjectField<
                            ẕvariants,
                            (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                            { ::zerocopy::STRUCT_VARIANT_ID },
                            { ::zerocopy::ident_id!(variants) },
                        > for ___ZerocopyRawEnum<'a, { N }, X, Y> {
                            fn only_derive_is_allowed_to_implement_this_trait() {}
                            type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                            type Invariants = (
                                Aliasing,
                                Alignment,
                                ::zerocopy::invariant::Initialized,
                            );
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
                unsafe impl<'a: 'static, X, Y: Deref, const N: usize> ::zerocopy::HasTag
                for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Tag = ___ZerocopyTag;
                    type ProjectToTag = ::zerocopy::pointer::cast::CastSized;
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::HasField<
                    (),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(a) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Type = u8;
                    #[inline(always)]
                    fn project(
                        slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                    ) -> *mut <Self as ::zerocopy::HasField<
                        (),
                        { ::zerocopy::ident_id!(StructLike) },
                        { ::zerocopy::ident_id!(a) },
                    >>::Type {
                        use ::zerocopy::pointer::cast::{CastSized, Projection};
                        slf.project::<___ZerocopyRawEnum<'a, N, X, Y>, CastSized>()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(variants) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                    { ::zerocopy::ident_id!(__field_StructLike) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(value) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(1) },
                                >,
                            >()
                            .as_ptr()
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    Aliasing: ::zerocopy::invariant::Aliasing,
                    Alignment: ::zerocopy::invariant::Alignment,
                    const N: usize,
                > ::zerocopy::ProjectField<
                    (),
                    (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(a) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                    type Invariants = (
                        Aliasing,
                        Alignment,
                        ::zerocopy::invariant::Initialized,
                    );
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::HasField<
                    (),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(b) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Type = X;
                    #[inline(always)]
                    fn project(
                        slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                    ) -> *mut <Self as ::zerocopy::HasField<
                        (),
                        { ::zerocopy::ident_id!(StructLike) },
                        { ::zerocopy::ident_id!(b) },
                    >>::Type {
                        use ::zerocopy::pointer::cast::{CastSized, Projection};
                        slf.project::<___ZerocopyRawEnum<'a, N, X, Y>, CastSized>()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(variants) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                    { ::zerocopy::ident_id!(__field_StructLike) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(value) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(2) },
                                >,
                            >()
                            .as_ptr()
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    Aliasing: ::zerocopy::invariant::Aliasing,
                    Alignment: ::zerocopy::invariant::Alignment,
                    const N: usize,
                > ::zerocopy::ProjectField<
                    (),
                    (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(b) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                    type Invariants = (
                        Aliasing,
                        Alignment,
                        ::zerocopy::invariant::Initialized,
                    );
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::HasField<
                    (),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(c) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Type = X::Target;
                    #[inline(always)]
                    fn project(
                        slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                    ) -> *mut <Self as ::zerocopy::HasField<
                        (),
                        { ::zerocopy::ident_id!(StructLike) },
                        { ::zerocopy::ident_id!(c) },
                    >>::Type {
                        use ::zerocopy::pointer::cast::{CastSized, Projection};
                        slf.project::<___ZerocopyRawEnum<'a, N, X, Y>, CastSized>()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(variants) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                    { ::zerocopy::ident_id!(__field_StructLike) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(value) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(3) },
                                >,
                            >()
                            .as_ptr()
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    Aliasing: ::zerocopy::invariant::Aliasing,
                    Alignment: ::zerocopy::invariant::Alignment,
                    const N: usize,
                > ::zerocopy::ProjectField<
                    (),
                    (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(c) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                    type Invariants = (
                        Aliasing,
                        Alignment,
                        ::zerocopy::invariant::Initialized,
                    );
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::HasField<
                    (),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(d) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Type = Y::Target;
                    #[inline(always)]
                    fn project(
                        slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                    ) -> *mut <Self as ::zerocopy::HasField<
                        (),
                        { ::zerocopy::ident_id!(StructLike) },
                        { ::zerocopy::ident_id!(d) },
                    >>::Type {
                        use ::zerocopy::pointer::cast::{CastSized, Projection};
                        slf.project::<___ZerocopyRawEnum<'a, N, X, Y>, CastSized>()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(variants) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                    { ::zerocopy::ident_id!(__field_StructLike) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(value) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(4) },
                                >,
                            >()
                            .as_ptr()
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    Aliasing: ::zerocopy::invariant::Aliasing,
                    Alignment: ::zerocopy::invariant::Alignment,
                    const N: usize,
                > ::zerocopy::ProjectField<
                    (),
                    (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(d) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                    type Invariants = (
                        Aliasing,
                        Alignment,
                        ::zerocopy::invariant::Initialized,
                    );
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::HasField<
                    (),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(e) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Type = [(X, Y); N];
                    #[inline(always)]
                    fn project(
                        slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                    ) -> *mut <Self as ::zerocopy::HasField<
                        (),
                        { ::zerocopy::ident_id!(StructLike) },
                        { ::zerocopy::ident_id!(e) },
                    >>::Type {
                        use ::zerocopy::pointer::cast::{CastSized, Projection};
                        slf.project::<___ZerocopyRawEnum<'a, N, X, Y>, CastSized>()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(variants) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                    { ::zerocopy::ident_id!(__field_StructLike) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(value) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(5) },
                                >,
                            >()
                            .as_ptr()
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    Aliasing: ::zerocopy::invariant::Aliasing,
                    Alignment: ::zerocopy::invariant::Alignment,
                    const N: usize,
                > ::zerocopy::ProjectField<
                    (),
                    (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                    { ::zerocopy::ident_id!(StructLike) },
                    { ::zerocopy::ident_id!(e) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                    type Invariants = (
                        Aliasing,
                        Alignment,
                        ::zerocopy::invariant::Initialized,
                    );
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::HasField<
                    (),
                    { ::zerocopy::ident_id!(TupleLike) },
                    { ::zerocopy::ident_id!(0) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Type = bool;
                    #[inline(always)]
                    fn project(
                        slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                    ) -> *mut <Self as ::zerocopy::HasField<
                        (),
                        { ::zerocopy::ident_id!(TupleLike) },
                        { ::zerocopy::ident_id!(0) },
                    >>::Type {
                        use ::zerocopy::pointer::cast::{CastSized, Projection};
                        slf.project::<___ZerocopyRawEnum<'a, N, X, Y>, CastSized>()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(variants) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                    { ::zerocopy::ident_id!(__field_TupleLike) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(value) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(1) },
                                >,
                            >()
                            .as_ptr()
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    Aliasing: ::zerocopy::invariant::Aliasing,
                    Alignment: ::zerocopy::invariant::Alignment,
                    const N: usize,
                > ::zerocopy::ProjectField<
                    (),
                    (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                    { ::zerocopy::ident_id!(TupleLike) },
                    { ::zerocopy::ident_id!(0) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                    type Invariants = (
                        Aliasing,
                        Alignment,
                        ::zerocopy::invariant::Initialized,
                    );
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::HasField<
                    (),
                    { ::zerocopy::ident_id!(TupleLike) },
                    { ::zerocopy::ident_id!(1) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Type = Y;
                    #[inline(always)]
                    fn project(
                        slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                    ) -> *mut <Self as ::zerocopy::HasField<
                        (),
                        { ::zerocopy::ident_id!(TupleLike) },
                        { ::zerocopy::ident_id!(1) },
                    >>::Type {
                        use ::zerocopy::pointer::cast::{CastSized, Projection};
                        slf.project::<___ZerocopyRawEnum<'a, N, X, Y>, CastSized>()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(variants) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                    { ::zerocopy::ident_id!(__field_TupleLike) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(value) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(2) },
                                >,
                            >()
                            .as_ptr()
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    Aliasing: ::zerocopy::invariant::Aliasing,
                    Alignment: ::zerocopy::invariant::Alignment,
                    const N: usize,
                > ::zerocopy::ProjectField<
                    (),
                    (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                    { ::zerocopy::ident_id!(TupleLike) },
                    { ::zerocopy::ident_id!(1) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                    type Invariants = (
                        Aliasing,
                        Alignment,
                        ::zerocopy::invariant::Initialized,
                    );
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    const N: usize,
                > ::zerocopy::HasField<
                    (),
                    { ::zerocopy::ident_id!(TupleLike) },
                    { ::zerocopy::ident_id!(2) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Type = PhantomData<&'a [(X, Y); N]>;
                    #[inline(always)]
                    fn project(
                        slf: ::zerocopy::pointer::PtrInner<'_, Self>,
                    ) -> *mut <Self as ::zerocopy::HasField<
                        (),
                        { ::zerocopy::ident_id!(TupleLike) },
                        { ::zerocopy::ident_id!(2) },
                    >>::Type {
                        use ::zerocopy::pointer::cast::{CastSized, Projection};
                        slf.project::<___ZerocopyRawEnum<'a, N, X, Y>, CastSized>()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(variants) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                    { ::zerocopy::ident_id!(__field_TupleLike) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(value) },
                                >,
                            >()
                            .project::<
                                _,
                                Projection<
                                    _,
                                    { ::zerocopy::STRUCT_VARIANT_ID },
                                    { ::zerocopy::ident_id!(3) },
                                >,
                            >()
                            .as_ptr()
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
                unsafe impl<
                    'a: 'static,
                    X,
                    Y: Deref,
                    Aliasing: ::zerocopy::invariant::Aliasing,
                    Alignment: ::zerocopy::invariant::Alignment,
                    const N: usize,
                > ::zerocopy::ProjectField<
                    (),
                    (Aliasing, Alignment, ::zerocopy::invariant::Initialized),
                    { ::zerocopy::ident_id!(TupleLike) },
                    { ::zerocopy::ident_id!(2) },
                > for ComplexWithGenerics<'a, { N }, X, Y>
                where
                    X: Deref<Target = &'a [(X, Y); N]>,
                {
                    fn only_derive_is_allowed_to_implement_this_trait() {}
                    type Error = ::zerocopy::util::macro_util::core_reexport::convert::Infallible;
                    type Invariants = (
                        Aliasing,
                        Alignment,
                        ::zerocopy::invariant::Initialized,
                    );
                }
            };
            let tag = {
                let tag_ptr = unsafe {
                    candidate
                        .reborrow()
                        .project_transmute_unchecked::<
                            _,
                            ::zerocopy::invariant::Initialized,
                            ::zerocopy::pointer::cast::CastSized,
                        >()
                };
                tag_ptr
                    .recall_validity::<_, (_, (_, _))>()
                    .read::<::zerocopy::BecauseImmutable>()
            };
            let mut raw_enum = candidate
                .cast::<
                    ::zerocopy::ReadOnly<___ZerocopyRawEnum<'a, N, X, Y>>,
                    ::zerocopy::pointer::cast::CastSized,
                    (::zerocopy::pointer::BecauseRead, _),
                >();
            let variants = ::zerocopy::into_inner!(
                raw_enum.project:: < _, { ::zerocopy::STRUCT_VARIANT_ID }, {
                ::zerocopy::ident_id!(variants) } > ()
            );
            match tag {
                ___ZEROCOPY_TAG_UnitLike => true,
                ___ZEROCOPY_TAG_StructLike => {
                    let variant_md = variants
                        .cast::<
                            _,
                            ::zerocopy::pointer::cast::Projection<
                                _,
                                { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                { ::zerocopy::ident_id!(__field_StructLike) },
                            >,
                            _,
                        >();
                    let variant = variant_md
                        .cast::<
                            ::zerocopy::ReadOnly<
                                ___ZerocopyVariantStruct_StructLike<'a, N, X, Y>,
                            >,
                            ::zerocopy::pointer::cast::CastSized,
                            (::zerocopy::pointer::BecauseRead, _),
                        >();
                    <___ZerocopyVariantStruct_StructLike<
                        'a,
                        N,
                        X,
                        Y,
                    > as ::zerocopy::TryFromBytes>::is_bit_valid(variant)
                }
                ___ZEROCOPY_TAG_TupleLike => {
                    let variant_md = variants
                        .cast::<
                            _,
                            ::zerocopy::pointer::cast::Projection<
                                _,
                                { ::zerocopy::REPR_C_UNION_VARIANT_ID },
                                { ::zerocopy::ident_id!(__field_TupleLike) },
                            >,
                            _,
                        >();
                    let variant = variant_md
                        .cast::<
                            ::zerocopy::ReadOnly<
                                ___ZerocopyVariantStruct_TupleLike<'a, N, X, Y>,
                            >,
                            ::zerocopy::pointer::cast::CastSized,
                            (::zerocopy::pointer::BecauseRead, _),
                        >();
                    <___ZerocopyVariantStruct_TupleLike<
                        'a,
                        N,
                        X,
                        Y,
                    > as ::zerocopy::TryFromBytes>::is_bit_valid(variant)
                }
                _ => false,
            }
        }
    }
};
