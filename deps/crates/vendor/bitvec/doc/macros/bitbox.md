# Boxed Bit-Slice Constructor

This macro creates encoded `BitSlice` buffers at compile-time, and at run-time
copies them directly into a new heap allocation.

It forwards all of its arguments to [`bitvec!`], and calls
[`BitVec::into_boxed_bitslice`] on the produced `BitVec`.

[`BitVec::into_boxed_bitslice`]: crate::vec::BitVec::into_boxed_bitslice
[`bitvec!`]: macro@crate::bitvec
