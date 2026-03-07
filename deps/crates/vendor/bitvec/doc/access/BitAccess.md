# Bit-Level Access Instructions

This trait extends [`Radium`] in order to manipulate specific bits in an element
according to the crate’s logic. It drives all memory access instructions and is
responsible for translating the bit-selection logic of the [`index`] module into
real effects.

This is blanket-implemented on all types that permit shared-mutable memory
access via the [`radium`] crate. Its use is constrained in the [`store`] module.
It is required to be a publicly accessible symbol, as it is exported in other
traits, but it is a crate-internal item and is not part of the public API. Its
blanket implementation for `<R: Radium>` prevents any other implementations from
being written.

## Implementation and Safety Notes

This trait is automatically implemented for all types that implement `Radium`,
and relies exclusively on `Radium`’s API and implementations for its work. In
particular, `Radium` has no functions which operate on **pointers**: it
exclusively operates on memory through **references**. Since references must
always refer to initialized memory, `BitAccess` and, by extension, all APIs in
`bitvec` that touch memory, cannot be used to operate on uninitialized memory in
any way.

While you may *create* a `bitvec` pointer object that targets uninitialized
memory, you may not *dereference* it until the targeted memory has been wholly
initialized with integer values.

This restriction cannot be loosened without stable access to pointer-based
atomic intrinsics in the Rust standard library and corresponding updates to the
`Radium` trait.

Do not attempt to access uninitialized memory through `bitvec`. Doing so will
cause `bitvec` to produce references to uninitialized memory, which is undefined
behavior.

[`Radium`]: radium::Radium
[`index`]: crate::index
[`radium`]: radium
[`store`]: crate::store
