# Unified Element Constructor

This type is a hack around the fact that `Cell` and `AtomicUN` all have
`const fn new(val: Inner) -> Self;` constructors, but the numberic fundamentals
do not. As such, the standard library does not provide a unified construction
syntax to turn an integer fundamental into the final type.

This provides a `const fn BitElement::<_>::new(R) -> Self;` function,
implemented only for the `BitStore` implementors that the crate provides, that
the constructor macros can use to turn integers into final values without using
[`mem::transmute`][0]. While `transmute` is acceptable in this case (the types
are all `#[repr(transparent)]`), it is still better avoided where possible.

As this is a macro assistant, it is publicly exposed, but is not public API. It
has no purpose outside of the crate’s macros.

[0]: core::mem::transmute.
