# Reading From a Bit-Slice

The implementation loads bytes out of the referenced bit-slice until either the
destination buffer is filled or the source has no more bytes to provide. When
`.read()` returns, the provided bit-slice handle will have been updated to no
longer include the leading segment copied out as bytes into `buf`.

Note that the return value of `.read()` is always the number of *bytes* of `buf`
filled!

The implementation uses [`BitField::load_be`] to collect bytes. Note that unlike
the standard library, it is implemented on bit-slices of *any* underlying
element type. However, using a `BitSlice<_, u8>` is still likely to be fastest.

## Original

[`impl Read for [u8]`][orig]

[orig]: https://doc.rust-lang.org/std/primitive.slice.html#impl-Read
[`BitField::load_be`]: crate::field::BitField::load_be
