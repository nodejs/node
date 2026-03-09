# Boxed Bit-Slice Iteration

This module contains the by-value iterator used by both `BitBox` and `BitVec`.
In the standard library, this iterator is defined under `alloc::vec`, not
`alloc::boxed`, as `Box` already has an iteration implementation that forwards
to its boxed value.

It is moved here for simplicity: both `BitBox` and `BitVec` iterate over a
dynamic bit-slice by value, and must deällocate the region when dropped. As
`BitBox` has a smaller value than `BitVec`, it is used as the owning handle for
the bit-slice being iterated.
