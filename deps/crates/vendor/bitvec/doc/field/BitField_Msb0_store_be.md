# `Msb0` Big-Endian Integer Storing

This implementation uses the `Msb0` bit-ordering to determine *which* bits in a
partially-occupied memory element are used for storage, using big-endian element
ordering.

See the [trait method definition][orig] for an overview of what element ordering
means.

## Narrowing Behavior

Integers are truncated from the high end. When storing into a bit-slice of
length `n`, the `n` least numerically significant bits are stored, and any
remaining high bits are ignored.

Be aware of this behavior if you are storing signed integers! The signed integer
`-14i8` (bit pattern `0b1111_0010u8`) will, when stored into and loaded back
from a 4-bit slice, become the value `2i8`.

## Examples

```rust
use bitvec::prelude::*;

let mut raw = 0u8;
raw.view_bits_mut::<Msb0>()
   [2 .. 7]
   .store_be(22u8);
assert_eq!(raw, 0b00_10110_0);
//                 01 23456 7
raw.view_bits_mut::<Msb0>()
   [2 .. 7]
   .store_be(-10i8);
assert_eq!(raw, 0b00_10110_0);
```

In bit-slices that span multiple elements, the big-endian element ordering means
that the slice index increases while numerical significance decreases:

```rust
use bitvec::prelude::*;

let mut raw = [!0u8; 3];
raw.view_bits_mut::<Msb0>()
   [4 .. 20]
   .store_be(0x2018u16);
assert_eq!(raw, [
  0xF_2,
//  0 7
  0x0_1,
//  8 15
  0x8_F,
// 16 23
]);
```

Note that while these examples use `u8` storage for convenience in displaying
the literals, `BitField` operates identically with *any* storage type. As most
machines use little-endian *byte ordering* within wider element types, and
`bitvec` exclusively operates on *elements*, the actual bytes of memory may
rapidly start to behave oddly when translating between numeric literals and
in-memory representation.

The [user guide] has a chapter that translates bit indices into memory positions
for each combination of `<T: BitStore, O: BitOrder>`, and may be of additional
use when choosing a combination of type parameters and store functions.

[orig]: crate::field::BitField::store_be
[user guide]: https://bitvecto-rs.github.io/bitvec/memory-layout
