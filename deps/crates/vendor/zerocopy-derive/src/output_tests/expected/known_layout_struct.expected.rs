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
    unsafe impl ::zerocopy::KnownLayout for Foo
    where
        Self: ::zerocopy::util::macro_util::core_reexport::marker::Sized,
    {
        fn only_derive_is_allowed_to_implement_this_trait() {}
        type PointerMetadata = ();
        type MaybeUninit = ::zerocopy::util::macro_util::core_reexport::mem::MaybeUninit<
            Self,
        >;
        const LAYOUT: ::zerocopy::DstLayout = ::zerocopy::DstLayout::for_type::<Self>();
        #[inline(always)]
        fn raw_from_ptr_len(
            bytes: ::zerocopy::util::macro_util::core_reexport::ptr::NonNull<u8>,
            _meta: (),
        ) -> ::zerocopy::util::macro_util::core_reexport::ptr::NonNull<Self> {
            bytes.cast::<Self>()
        }
        #[inline(always)]
        fn pointer_to_metadata(_ptr: *mut Self) -> () {}
    }
};
