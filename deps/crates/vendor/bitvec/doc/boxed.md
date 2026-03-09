# Heap-Allocated, Fixed-Size, Bit Buffer

This module defines an analogue to `Box<[bool]>`, as `Box<BitSlice>` cannot be
constructed or used in any way. Like `Box<[T]>`, this is a heap allocation that
can modify its contents, but cannot resize the collection. The `BitBox` value is
an owning [`*BitSlice`] pointer, and can be used to access its contents without
any decoding.

You should generally prefer [`BitVec`] or [`BitArray`]; however, very large
`BitArrays` are likely better served being copied into a `BitBox` rather than
being boxed themselves when moved into the heap.

[`BitArray`]: crate::array::BitArray
[`BitVec`]: crate::vec::BitVec
[`*BitSlice`]: crate::slice::BitSlice
