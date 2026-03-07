# Writing Into a Bit-Slice

The implementation stores bytes into the referenced bit-slice until either the
source buffer is exhausted or the destination has no more slots to fill. When
`.write()` returns, the provided bit-slice handle will have been updated to no
longer include the leading segment filled with bytes from `buf`.

Note that the return value of `.write()` is always the number of *bytes* of
`buf` consumed!

The implementation uses [`BitField::store_be`] to fill bytes. Note that unlike
the standard library, it is implemented on bit-slices of *any* underlying
element type. However, using a `BitSlice<_, u8>` is still likely to be fastest.

## Original

[`impl Write for [u8]`][orig]

[orig]: https://doc.rust-lang.org/std/primitive.slice.html#impl-Write
[`BitField::store_be`]: crate::field::BitField::store_be
