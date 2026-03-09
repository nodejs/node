# De/Serialization Assistants

This module contains types and implementations that assist in the
de/serialization of the crate’s primary data structures.

## `BitIdx<R>`

The `BitIdx` implementation serializes both the index value and also the
bit-width of `T::Mem`, so that the deserializer can ensure that it only loads
from a matching data buffer.

## `Array<T, N>`

Serde only provides implementations for `[T; 0 ..= 32]`, because it must support
much older Rust versions (at time of writing, 1.15+) that do not have
const-generics. As `bitvec` has an MSRV of 1.56; it *does* have const-generics.
This type reïmplements Serde’s array behavior for all arrays, so that `bitvec`
can transport any `BitArray` rather than only small bit-arrays.

## `Domain<Const, T, O>`

`BitSlice` serializes its data buffer by using `Domain` to produce a sequence of
elements. While the length is always known, and is additionally carried in the
crate metadata ahead of the data buffer, `Domain` uses Serde’s sequence model in
order to allow the major implementations to use the provided slice or vector
deserializers, rather than rebuilding even more logic from scratch.
