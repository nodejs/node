# Bit Indices

This module provides well-typed counters for working with bit-storage registers.
The session types encode a strict chain of custody for translating semantic
indices within [`BitSlice`] regions into real effects in memory.

The main advantage of types within this module is that they provide
register-dependent range requirements for counter values, making it impossible
to have an index out of bounds for a register. They also create a sequence of
type transformations that assure the library about the continued validity of
each value in its surrounding context.

By eliminating public constructors from arbitrary integers, `bitvec` can
guarantee that only it can produce initial values, and only trusted functions
can transform their numeric values or types, until the program reaches the
property that it requires. This chain of assurance means that memory operations
can be confident in the correctness of their actions and effects.

## Type Sequence

The library produces [`BitIdx`] values from region computation. These types
cannot be publicly constructed, and are only ever the result of pointer
analysis. As such, they rely on the correctness of the memory regions provided
to library entry points, and those entry points can leverage the Rust type
system to ensure safety there.

`BitIdx` is transformed to [`BitPos`] through the [`BitOrder`] trait. The
[`order`] module provides verification functions that implementors can use to
demonstrate correctness. `BitPos` is the basis type that describes memory
operations, and is used to create the selection masks [`BitSel`] and
[`BitMask`].

## Usage

The types in this module should only be used by client crates in their test
suites. They have no other purpose, and conjuring values for them is potentially
memory-unsafe.

[`BitIdx`]: self::BitIdx
[`BitMask`]: self::BitMask
[`BitOrder`]: crate::order::BitOrder
[`BitPos`]: self::BitPos
[`BitSel`]: self::BitSel
[`BitSlice`]: crate::slice::BitSlice
[`order`]: crate::order
