# Big-Endian Integer Loading

This method loads an integer value from a bit-slice, using big-endian
significance ordering when the bit-slice spans more than one `T` element in
memory.

Big-endian significance ordering means that if a bit-slice occupies an array
`[A, B, C]`, then the bits stored in `A` are considered to be the most
significant segment of the loaded integer, then `B` contains the middle segment,
then `C` contains the least significant segment.

The segments are combined in order, that is, as the raw bit-pattern
`0b<padding><A><B><C>`. If the destination type is signed, the loaded value is
sign-extended according to the most-significant bit in the `A` segment.

It is important to note that the `O: BitOrder` parameter of the bit-slice from
which the value is loaded **does not** affect the bit-pattern of the stored
segments. They are always stored exactly as they exist in an ordinary integer.
The ordering parameter only affects *which* bits in an element are available for
storage.

## Type Parameters

- `I`: The integer type being loaded. This can be any of the signed or unsigned
  integers.

## Parameters

- `&self`: A bit-slice region whose length is in the range `1 ..= I::BITS`.

## Returns

The contents of the bit-slice, interpreted as an integer.

## Panics

This panics if `self.len()` is 0, or greater than `I::BITS`.

## Examples

Let us consider an `i32` value stored in 24 bits of a `BitSlice<u8, Lsb0>`:

```rust
use bitvec::prelude::*;

let mut raw = [0u8; 4];
let bits = raw.view_bits_mut::<Lsb0>();

let integer = 0x00__B4_96_3Cu32 as i32;
bits[4 .. 28].store_be::<i32>(integer);
let loaded = bits[4 .. 28].load_be::<i32>();
assert_eq!(loaded, 0xFF__B4_96_3Cu32 as i32);
```

Observe that, because the lowest 24 bits began with the pattern `0b1101…`, the
value was considered to be negative when interpreted as an `i24` and was
sign-extended through the highest byte.

Let us now look at the memory representation of this value:

```rust
# use bitvec::prelude::*;
# let mut raw = [0u8; 4];
# let bits = raw.view_bits_mut::<Lsb0>();
# bits[4 .. 28].store_be::<u32>(0x00B4963Cu32);
assert_eq!(raw, [
  0b1011_0000,
//  0xB  dead
  0b0100_1001,
//  0x4  0x9
  0b0110_0011,
//  0x6  0x3
  0b0000_1100,
//  dead 0xC
]);
```

Notice how while the `Lsb0` bit-ordering means that indexing within the
bit-slice proceeds right-to-left in each element, the actual bit-patterns stored
in memory are not affected. Element `[0]` is more numerically significant than
element `[1]`, but bit `[4]` is not more numerically significant than bit `[5]`.

In the sequence `B496`, `B` is the most significant, and so it gets placed
lowest in memory. `49` fits in one byte, and is stored directly as written.
Lastly, `6` is the least significant nibble of the four, and is placed highest
in memory.

Now let’s look at the way different `BitOrder` parameters interpret the
placement of bit indices within memory:

```rust
use bitvec::prelude::*;

let raw = [
// Bit index   14 ←
//     Lsb0:  ─┤
            0b0100_0000_0000_0011u16,
//     Msb0:                   ├─
//                           → 14

// Bit index               ← 19 16
//     Lsb0:                 ├──┤
            0b0001_0000_0000_1110u16,
//     Msb0:  ├──┤
//            16 19 →
];

assert_eq!(
  raw.view_bits::<Lsb0>()
     [14 .. 20]
     .load_be::<u8>(),
  0b00_01_1110,
);
assert_eq!(
  raw.view_bits::<Msb0>()
     [14 .. 20]
     .load_be::<u8>(),
  0b00_11_0001,
);
```

Notice how the bit-orderings change which *parts* of the memory are loaded, but
in both cases the segment in `raw[0]` is more significant than the segment in
`raw[1]`, and the ordering of bits *within* each segment are unaffected by the
bit-ordering.

## Notes

Be sure to see the documentation for
[`<BitSlice<_, Lsb0> as BitField>::load_be`][llb] and
[`<BitSlice<_, Msb0> as Bitfield>::load_be`][mlb] for more detailed information
on the memory views!

You can view the mask of all *storage regions* of a bit-slice by using its
[`.domain()`] method to view the breakdown of its memory region, then print the
[`.mask()`] of any [`PartialElement`] the domain contains. Whole elements are
always used in their entirety. You should use the `domain` module’s types
whenever you are uncertain of the exact locations in memory that a particular
bit-slice governs.

[llb]: https://docs.rs/bitvec/latest/bitvec/field/trait.BitField.html#method.load_be-3
[mlb]: https://docs.rs/bitvec/latest/bitvec/field/trait.BitField.html#method.load_be-4
[`PartialElement`]: crate::domain::PartialElement
[`.domain()`]: crate::slice::BitSlice::domain
[`.mask()`]: crate::domain::PartialElement::mask
