impl<T: Clone> ::zerocopy::util::macro_util::core_reexport::hash::Hash for Foo<T>
where
    Self: ::zerocopy::IntoBytes + ::zerocopy::Immutable,
    Self: Sized,
{
    fn hash<H: ::zerocopy::util::macro_util::core_reexport::hash::Hasher>(
        &self,
        state: &mut H,
    ) {
        ::zerocopy::util::macro_util::core_reexport::hash::Hasher::write(
            state,
            ::zerocopy::IntoBytes::as_bytes(self),
        )
    }
    fn hash_slice<H: ::zerocopy::util::macro_util::core_reexport::hash::Hasher>(
        data: &[Self],
        state: &mut H,
    ) {
        ::zerocopy::util::macro_util::core_reexport::hash::Hasher::write(
            state,
            ::zerocopy::IntoBytes::as_bytes(data),
        )
    }
}
