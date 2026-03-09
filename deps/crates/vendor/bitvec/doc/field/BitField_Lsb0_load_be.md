# `Lsb0` Big-Endian Integer Loading

This implementation uses the `Lsb0` bit-ordering to determine *which* bits in a
partially-occupied memory element contain the contents of an integer to be
loaded, using big-endian element ordering.

See the [trait method definition][orig] for an overview of what element ordering
means.

## Signed-Integer Loading

As described in the trait definition, when loading as a signed integer, the most
significant bit *loaded* from memory is sign-extended to the full width of the
returned type. In this method, that means that the most-significant bit of the
first element.

## Examples

In each memory element, the `Lsb0` ordering counts indices leftward from the
right edge:

```rust
use bitvec::prelude::*;

let raw = 0b00_10110_0u8;
//           76 54321 0
//              ^ sign bit
assert_eq!(
  raw.view_bits::<Lsb0>()
     [1 .. 6]
     .load_be::<u8>(),
  0b000_10110,
);
assert_eq!(
  raw.view_bits::<Lsb0>()
     [1 .. 6]
     .load_be::<i8>(),
  0b111_10110u8 as i8,
);
```

In bit-slices that span multiple elements, the big-endian element ordering means
that the slice index increases while numeric significance decreases:

```rust
use bitvec::prelude::*;

let raw = [
  0b0010_1111u8,
//  ^ sign bit
//  7       0
  0x0_1u8,
// 15 8
  0xF_8u8,
// 23 16
];
assert_eq!(
  raw.view_bits::<Lsb0>()
     [4 .. 20]
     .load_be::<u16>(),
  0x2018u16,
);
```

Note that while these examples use `u8` storage for convenience in displaying
the literals, `BitField` operates identically with *any* storage type. As most
machines use little-endian *byte ordering* within wider element types, and
`bitvec` exclusively operates on *elements*, the actual bytes of memory may
rapidly start to behave oddly when translating between numeric literals and
in-memory representation.

The [user guide] has a chapter that translates bit indices into memory positions
for each combination of `<T: BitStore, O: BitOrder>`, and may be of additional
use when choosing a combination of type parameters and load functions.

[orig]: crate::field::BitField::load_le
[user guide]: https://bitvecto-rs.github.io/bitvec/memory-layout
