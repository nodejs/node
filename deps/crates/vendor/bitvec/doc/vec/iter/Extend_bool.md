# Bit-Vector Extension

This extends a bit-vector from anything that produces individual bits.

## Original

[`impl<T> Extend<T> for Vec<T>`][orig]

## Notes

This `.extend()` call is the second-slowest possible way to append bits into a
bit-vector, faster only than calling `iter.for_each(|bit| bv.push(bit))`.
**DO NOT** use this if you have any other choice.

If you are extending a bit-vector from the contents of a bit-slice, then you
should use [`.extend_from_bitslice()`] instead. That method is specialized to
perform upfront allocation and, where possible, use a batch copy rather than
copying each bit individually from the source into the bit-vector.

[orig]: https://doc.rust-lang.org/alloc/vec/struct.Vec.html#impl-Extend%3CT%3E
[`.extend_from_bitslice()`]: crate::vec::BitVec::extend_from_bitslice
