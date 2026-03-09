# Bit-Pointer Range

This type is equivalent in purpose, but superior in functionality, to
`Range<BitPtr<M, T, O>>`. If the standard library stabilizes [`Step`], the trait
used to drive `Range` operations, then this type will likely be destroyed in
favor of an `impl Step for BitPtr` block and use of standard ranges.

Like [`Range`], this is a half-open set where the low bit-pointer selects the
first live bit in a span and the high bit-pointer selects the first dead bit
*after* the span.

This type is not capable of inspecting provenance, and has no requirement of its
own that both bit-pointers be derived from the same provenance region. It is
safe to construct and use with any pair of bit-pointers; however, the
bit-pointers it *produces* are, necessarily, `unsafe` to use.

## Original

[`Range<*bool>`][`Range`]

## Memory Representation

[`BitPtr`] is required to be `repr(packed)` in order to satisfy the [`BitRef`]
size optimizations. In order to stay minimally sized itself, this type has no
alignment requirement, and reading either bit-pointer *may* incur a misalignment
penalty. Reads are always safe and valid; they may merely be slow.

## Type Parameters

This takes the same type parameters as `BitPtr`, as it is simply a pair of
bit-pointers with range semantics.

[`BitPtr`]: crate::ptr::BitPtr
[`BitRef`]: crate::ptr::BitRef
[`Range`]: core::ops::Range
[`Step`]: core::iter::Step
