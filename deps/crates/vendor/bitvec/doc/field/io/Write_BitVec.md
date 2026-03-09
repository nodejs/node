# Writing Into a Bit-Vector

The implementation appends bytes to the referenced bit-vector until the source
buffer is exhausted.

Note that the return value of `.write()` is always the number of *bytes* of
`buf` consumed!

The implementation uses [`BitField::store_be`] to fill bytes. Note that unlike
the standard library, it is implemented on bit-vectors of *any* underlying
element type. However, using a `BitVec<_, u8>` is still likely to be fastest.

## Original

[`impl Write for Vec<u8>`][orig]

[orig]: https://doc.rust-lang.org/std/vec/struct.Vec.html#impl-Write
[`BitField::store_be`]: crate::field::BitField::store_be
