# Raw Pointer Implementation

This provides `bitvec`-internal pointer types and a mirror of the [`core::ptr`]
module.

It contains the following types:

- [`BitPtr`] is a raw-pointer to exactly one bit.
- [`BitRef`] is a proxy reference to exactly one bit.
- `BitSpan` is the encoded form of the `*BitSlice` pointer and `&BitSlice`
  reference. It is not publicly exposed, but it serves as the foundation of
  `bitvec`’s ability to describe memory regions.

It also provides ports of the free functions available in `core::ptr`, as well
as some utilities for bridging ordinary Rust pointers into `bitvec`.

You should generally not use the contents of this module; `BitSlice` provides
more convenience and has stronger abilities to optimize performance.

[`BitPtr`]: self::BitPtr
[`BitRef`]: self::BitRef
[`core::ptr`]: core::ptr
