# Bit-Field Memory Slots

This module implements a load/store protocol for [`BitSlice`] regions that
enables them to act as if they were a storage slot for integers. Implementations
of the [`BitField`] trait provide behavior similar to C and C++ language
bit-fields. While any `BitSlice<T, O>` instantiation is able to provide this
behavior, the lack of specialization in the language means that it is instead
only implemented for `BitSlice<T, Lsb0>` and `BitSlice<T, Msb0>` in order to
gain a performance advantage.

## Batched Behavior

Bit-field behavior can be simulated using `BitSlice`’s existing APIs; however,
the inherent methods are all required to operate on each bit individually in
sequence. In addition to the semantic load/store behavior this module describes,
it also implements it in a way that takes advantage of the contiguity properties
of the `Lsb0` and `Msb0` orderings in order to maximize how many bits are
transferred in each cycle of the overall operation.

This is most efficient when using `BitSlice<usize, O>` as the storage bit-slice,
or using `.load::<usize>()` or `.store::<usize>()` as the transfer type.

## Bit-Slice Storage and Integer Value Relationships

`BitField` permits any type of integer, *including signed integers*, to be
stored into or loaded out of a `BitSlice<T, _>` with any storage type `T`. While
the examples in this module will largely use `u8`, just to keep the text
concise, `BitField` is tested, and will work correctly, for any combination of
types.

`BitField` implementations use the processor’s own concept of integer registers
to operate. As such, the byte-wise memory access patters for types wider than
`u8` depends on your processor’s byte endianness, as well as which `BitField`
method, and which [`BitOrder`] type parameter, you are using.

`BitField` only operates within processor registers; traffic of `T` elements
between the memory bank and the processor register is controlled entirely by the
processor.

If you do not want to introduce the processor’s byte endianness as a variable
that affects the in-memory representation of stored integers, use
`BitSlice<u8, _>` as the bit-field storage type. In particular,
`BitSlice<u8, Msb0>` will fill memory in a way that intuitively matches what
most debuggers show when inspecting memory.

On the other hand, if you do not care about memory representation and just need
fast storage of less than an entire integer, `BitSlice<Lsb0, usize>` is likely
your best bet. As always, the choice of type parameters is a trade-off with
different advantages for each combination, which is why `bitvec` refuses to make
the choice for you.

### Signed Behavior

The length of the `BitSlice` that stores a value is considered to be the width
of that value when it is loaded back out. As such, storing an `i16` into a
bit-slice of length `12` means that the stored value has type `i12`.

When calling `.load::<i16>()` on a 12-bit slice, the load will detect the sign
bit of the `i12` value and sign-extend it to `i16`. This means that storing
`2048i16` into a 12-bit slice and then loading it back out into an `i16` will
produce `-2048i16` (negative), not `2048i16` (positive), because `1 << 11` is
the sign bit.

`BitField` **does not** record the true sign bit of an integer being stored, and
will not attempt to set the sign bit of the narrowed value in storage. Storing
`-127i8` (`0b1000_0001`) into a 7-bit slice will load `1i8`.

## Register Bit Order Preservation

The implementations in this module assume that the bits within a *value* being
transferred into or out of a bit-slice should not be re-ordered. While the
implementations will segment a value in order to make it fit into bit-slice
storage, and will order those *segments* in memory according to their type
parameter and specific trait method called, each segment will remain
individually unmodified.

If we consider the value `0b100_1011`, segmented at the underscore, then the
segments `0b100` and `0b1011` will be present somewhere in the bit-slice that
stores them. They may be shifted within an element or re-ordered across
elements, but each segment will not be changed.

## Endianness

`bitvec` uses the `BitOrder` trait to describe the order of bits within a single
memory element. This ordering is independent of, and does not consider, the
ordering of memory elements in a sequence; `bitvec` is always “little-endian” in
this regard: lower indices are in lower memory addresses, higher indices are in
higher memory addresses.

However, `BitField` is *explicitly* aware of multiple storage elements in
sequence. It is by design able to allow combinations such as
`<BitSlice<u8, Lsb0> as BitField>::store_be::<u32>`. Even where the storage and
value types are the same, or the value is narrower, the bit-slice may be spread
across multiple elements and must segment the value across them.

The `_be` and `_le` orderings on `BitField` method names refer to the numeric
significance of *bit-slice storage elements*.

In `_be` methods, lower-address storage elements will hold more-significant
segments of the value, and higher-address storage will hold less-significant.

In `_le` methods, lower-address storage elements will hold *less*-significant
segments of the value, and higher-address storage will hold *more*-significant.

Consider again the value `0b100_1011`, segmented at the underscore. When used
with `.store_be()`, it will be placed into memory as `[0b…100…, 0b…1011…]`; when
used with `.store_le()`, it will be placed into memory as `[0b…1011…, 0b…100…]`.

## Bit-Ordering Behaviors

The `_be` and `_le` suffices select the ordering of storage elements in memory.
The other critical aspect of the `BitField` memory behavior is selecting
*which bits* in a storage element are used when a bit-slice has partial
elements.

When `BitSlice<_, Lsb0>` produces a [`Domain::Region`], its `head` is in the
most-significant bits of its element and its `tail` is in the least-significant
bits. When `BitSlice<_, Msb0>` produces a `Region`, its `head` is in the
*least*-significant bits, and its `tail` is in the *most*-significant bits.

You can therefore use these combinations of `BitOrder` type parameter and
`BitField` method suffix to select exactly the memory behavior you want for a
storage region.

Each implementation of `BitField` has documentation showing exactly what its
memory layout looks like, with code examples and visual inspections of memory.
This documentation is likely collapsed by default when viewing the trait docs;
be sure to use the `[+]` button to expand it!

[`BitField`]: self::BitField
[`BitOrder`]: crate::order::BitOrder
[`BitSlice`]: crate::slice::BitSlice
[`Domain::Region`]: crate::domain::Domain::Region
