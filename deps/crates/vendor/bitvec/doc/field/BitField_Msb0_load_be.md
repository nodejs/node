# `Msb0` Big-Endian Integer Loading

This implementation uses the `Msb0` bit-ordering to determine *which* bits in a
partially-occupied memory element contain the contents of an integer to be
loaded, using big-endian element ordering.

See the [trait method definition][orig] for an overview of what element ordering
means.

## Signed-Integer Loading

As described in the trait definition, when loading as a signed integer, the most
significant bit *loaded* from memory is sign-extended to the full width of the
returned type. In this method, that means the most-significant loaded bit of the
first element.

## Examples

In each memory element, the `Msb0` ordering counts indices rightward from the
left edge:

```rust
use bitvec::prelude::*;

let raw = 0b00_10110_0u8;
//           01 23456 7
//              ^ sign bit
assert_eq!(
  raw.view_bits::<Msb0>()
     [2 .. 7]
     .load_be::<u8>(),
  0b000_10110,
);
assert_eq!(
  raw.view_bits::<Msb0>()
     [2 .. 7]
     .load_be::<i8>(),
  0b111_10110u8 as i8,
);
```

In bit-slices that span multiple elements, the big-endian element ordering means
that the slice index increases with numerical significance:

```rust
use bitvec::prelude::*;

let raw = [
  0b1111_0010u8,
//       ^ sign bit
//  0       7
  0x0_1u8,
//  8 15
  0x8_Fu8,
// 16 23
];
assert_eq!(
  raw.view_bits::<Msb0>()
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
