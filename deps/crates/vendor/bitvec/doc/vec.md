# Dynamically-Allocated, Adjustable-Size, Bit Buffer

This module defines the [`BitVec`] buffer and its associated support code.

`BitVec` is analogous to [`Vec<bool>`] in its use of dynamic memory allocation
and its relationship to the [`BitSlice`] type. Most of the interesting work to
be done on a bit sequence is actually implemented in `BitSlice`, with `BitVec`
itself largely only containing interfaces to the memory allocator.

## Original

[`vec`](mod@alloc::vec)

[`BitVec`]: crate::vec::BitVec
[`BitSlice`]: crate::slice::BitSlice
[`Vec<bool>`]: alloc::vec::Vec
