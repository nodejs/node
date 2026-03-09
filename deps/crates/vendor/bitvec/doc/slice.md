# Bit-Addressable Memory Regions

This module defines the [`BitSlice`] region, which forms the primary export item
of the crate. It is a region of memory that addresses each bit individually, and
is analogous to the slice language item. See `BitSlice`’s documentation for
information on its use.

The other data structures `bitvec` offers are built atop `BitSlice`, and follow
the development conventions outlined in this module. Because the API surface for
`bitvec` data structures is so large, they are broken into a number of common
submodules:

- `slice` defines the `BitSlice` data structure, its inherent methods
  that are original to `bitvec`, as well as some free functions.
- `slice::api` defines ports of the `impl<T> [T]` inherent blocks from
  `core::slice`.
- `slice::iter` contains all the logic used to iterate across `BitSlices`,
  including ports of `core::slice` iterators.
- `slice::ops` contains implementations of `core::ops` traits that power
  operator sigils.
- `slice::traits` contains all the other trait implementations.
- `slice::tests` contains unit tests for `BitSlice` inherent methods.

Additionally, `slice` has a submodule unique to it: `specialization` contains
override functions that provide faster behavior on known `BitOrder`
implementations. Since the other data structures `Deref` to it, they do not need
to implement bit-order specializations of their own.

All ports of language or standard-library items have an `## Original` section in
their documentation that links to the item they are porting, and possibly an
`## API Differences` that explains why the `bitvec` item is not a drop-in
replacement.

[`BitSlice`]: self::BitSlice
