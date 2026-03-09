# Statically-Allocated, Fixed-Size, Bit Buffer

This module defines a port of the [array fundamental][0] and its APIs. The
primary export is the [`BitArray`] structure. This is a thin wrapper over
`[T; N]` that provides a [`BitSlice`] view of its contents and is *roughly*
analogous to the C++ type [`std::bitset<N>`].

See the `BitArray` documentation for more details on its usage.

## Submodules

- `api` contains ports of the standard library’s array type and `core::array`
  module.
- `iter` contains ports of array iteration.
- `ops` defines operator-sigil traits.
- `traits` defines all the other traits.

[0]: https://doc.rust-lang.org/std/primitive.array.html
[`BitArray`]: self::BitArray
[`BitSlice`]: crate::slice::BitSlice
[`std::bitset<N>`]: https://en.cppreference.com/w/cpp/utility/bitset
