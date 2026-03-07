# Bit-Vector Constructor

This macro creates encoded `BitSlice` buffers at compile-time, and at run-time
copies them directly into a new heap allocation.

It forwards all of its arguments to [`bits!`], and calls
[`BitVec::from_bitslice`] on the produced `&BitSlice` expression. While you can
use the `bits!` modifiers, there is no point, as the produced bit-slice is lost
before the macro exits.

[`BitVec::from_bitslice`]: crate::vec::BitVec::from_bitslice
[`bits!`]: macro@crate::bits
