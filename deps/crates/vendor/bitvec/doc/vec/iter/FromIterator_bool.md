# Bit-Vector Collection

This collects a bit-vector from anything that produces individual bits.

## Original

[`impl<T> FromIterator<T> for Vec<T>`][orig]

## Notes

This `.collect()` call is the second-slowest possible way to collect bits into a
bit-vector, faster only than calling `iter.for_each(|bit| bv.push(bit))`.
**DO NOT** use this if you have any other choice.

If you are collecting a bit-vector from the contents of a bit-slice, then you
should use [`::from_bitslice()`] instead. That method is specialized to
perform upfront allocation and, where possible, use a batch copy rather than
copying each bit individually from the source into the bit-vector.

[orig]: https://doc.rust-lang.org/alloc/vec/struct.Vec.html#impl-FromIterator%3CT%3E
[`::from_bitslice()`]: crate::vec::BitVec::extend_from_bitslice
