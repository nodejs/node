# Bit-Precision Array Immediate

This type is a wrapper over the [array fundamental][0] `[T; N]` that views its
contents as a [`BitSlice`] region. As an array, it can be held directly by value
and does not require an indirection such as the `&BitSlice` reference.

## Original

[`[T; N]`](https://doc.rust-lang.org/std/primitive.array.html)

## Usage

`BitArray` is a Rust analogue of the C++ [`std::bitset<N>`] container. However,
restrictions in the Rust type system do not allow specifying exact bit lengths
in the array type. Instead, it must specify a storage array that can contain all
the bits you want.

Because `BitArray` is a plain-old-data object, its fields are public and it has
no restrictions on its interior value. You can freely access the interior
storage and move data in or out of the `BitArray` type with no cost.

As a convenience, the [`BitArr!`] type-constructor macro can produce correct
type definitions from an exact bit count and your memory-layout type parameters.
Values of that type can then be built from the [`bitarr!`] *value*-constructor
macro:

```rust
use bitvec::prelude::*;

type Example = BitArr!(for 43, in u32, Msb0);
let example: Example = bitarr!(u32, Msb0; 1; 33);

struct HasBitfield {
  inner: Example,
}

let ex2 = HasBitfield {
  inner: BitArray::new([1, 2]),
};
```

Note that the actual type of the `Example` alias is `BitArray<[u32; 2], Msb0>`,
as that is `ceil(32, 43)`, so the `bitarr!` macro can accept any number of bits
in `33 .. 65` and will produce a value of the correct type.

## Type Parameters

`BitArray` differs from the other data structures in the crate in that it does
not take a `T: BitStore` parameter, but rather takes `A: BitViewSized`. That
trait is implemented by all `T: BitStore` scalars and all `[T; N]` arrays of
them, and provides the logic to translate the aggregate storage into the memory
sequence that the crate expects.

As with all `BitSlice` regions, the `O: BitOrder` parameter specifies the
ordering of bits within a single `A::Store` element.

## Future API Changes

Exact bit lengths cannot be encoded into the `BitArray` type until the
const-generics system in the compiler can allow type-level computation on type
integers. When this stabilizes, `bitvec` will issue a major upgrade that
replaces the `BitArray<A, O>` definition with `BitArray<T, O, const N: usize>`
and match the C++ `std::bitset<N>` definition.

## Large Bit-Arrays

As with ordinary arrays, large arrays can be expensive to move by value, and
should generally be preferred to have static locations such as actual `static`
bindings, a long lifetime in a low stack frame, or a heap allocation. While you
certainly can `Box<[BitArray<A, O>]>` directly, you may instead prefer the
[`BitBox`] or [`BitVec`] heap-allocated regions. These offer the same storage
behavior and are better optimized than `Box<BitArray>` for working with the
contained `BitSlice` region.

## Examples

```rust
use bitvec::prelude::*;

const WELL_KNOWN: BitArr!(for 16, in u8, Lsb0) = BitArray::<[u8; 2], Lsb0> {
  data: *b"bv",
  ..BitArray::ZERO
};

struct HasBitfields {
  inner: BitArr!(for 50, in u8, Lsb0),
}

impl HasBitfields {
  fn new() -> Self {
    Self {
      inner: bitarr!(u8, Lsb0; 0; 50),
    }
  }

  fn some_field(&self) -> &BitSlice<u8, Lsb0> {
    &self.inner[2 .. 52]
  }
}
```

[0]: https://doc.rust-lang.org/std/primitive.array.html
[`BitArr!`]: macro@crate::BitArr
[`BitBox`]: crate::boxed::BitBox
[`BitSlice`]: crate::slice::BitSlice
[`BitVec`]: crate::vec::BitVec
[`bitarr!`]: macro@crate::bitarr
[`std::bitset<N>`]: https://en.cppreference.com/w/cpp/utility/bitset
