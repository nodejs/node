# Little-Endian Integer Storing

This method stores an integer value into a bit-slice, using little-endian
significance ordering when the bit-slice spans more than one `T` element in
memory.

Little-endian significance ordering means that if a bit-slice occupies an array
`[A, B, C]`, then the bits stored in `A` are considered to contain the least
significant segment of the stored integer, then `B` contains the middle segment,
and then `C` contains the most significant segment.

An integer is broken into segments in order, that is, the raw bit-pattern is
fractured into `0b<padding><C><B><A>`. High bits beyond the length of the
bit-slice into which the integer is stored are truncated.

It is important to note that the `O: BitOrder` parameter of the bit-slice into
which the value is stored **does not** affect the bit-pattern of the stored
segments. They are always stored exactly as they exist in an ordinary integer.
The ordering parameter only affects *which* bits in an element are available for
storage.

## Type Parameters

- `I`: The integer type being stored. This can be any of the signed or unsigned
  integers.

## Parameters

- `&mut self`: A bit-slice region whose length is in the range `1 ..= I::BITS`.
- `value`: An integer value whose `self.len()` least numerically significant
  bits will be written into `self`.

## Panics

This panics if `self.len()` is 0, or greater than `I::BITS`.

## Examples

Let us consider an `i32` value stored in 24 bits of a `BitSlice<u8, Msb0>`:

```rust
use bitvec::prelude::*;

let mut raw = [0u8; 4];
let bits = raw.view_bits_mut::<Msb0>();

let integer = 0x00__B4_96_3Cu32 as i32;
bits[4 .. 28].store_le::<i32>(integer);
let loaded = bits[4 .. 28].load_le::<i32>();
assert_eq!(loaded, 0xFF__B4_96_3Cu32 as i32);
```

Observe that, because the lowest 24 bits began with the pattern `0b1101…`, the
value was considered to be negative when interpreted as an `i24` and was
sign-extended through the highest byte.

Let us now look at the memory representation of this value:

```rust
# use bitvec::prelude::*;
# let mut raw = [0u8; 4];
# let bits = raw.view_bits_mut::<Msb0>();
# bits[4 .. 28].store_le::<u32>(0x00B4963Cu32);
assert_eq!(raw, [
  0b0000_1100,
//  dead 0xC
  0b0110_0011,
//  0x6  0x3
  0b0100_1001,
//  0x4  0x9
  0b1011_0000,
//  0xB  dead
]);
```

Notice how while the `Msb0` bit-ordering means that indexing within the
bit-slice proceeds left-to-right in each element, and the bit-patterns in each
element proceed left-to-right in the aggregate and the decomposed literals, the
ordering of the elements is reversed from how the literal was written.

In the sequence `B496`, `B` is the most significant, and so it gets placed
highest in memory. `49` fits in one byte, and is stored directly as written.
Lastly, `6` is the least significant nibble of the four, and is placed lowest
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
     .load_le::<u8>(),
  0b00_1110_01,
);
assert_eq!(
  raw.view_bits::<Msb0>()
     [14 .. 20]
     .load_le::<u8>(),
  0b00_0001_11,
);
```

Notice how the bit-orderings change which *parts* of the memory are loaded, but
in both cases the segment in `raw[0]` is less significant than the segment in
`raw[1]`, and the ordering of bits *within* each segment are unaffected by the
bit-ordering.

## Notes

Be sure to see the documentation for
[`<BitSlice<_, Lsb0> as BitField>::store_le`][lsl] and
[`<BitSlice<_, Msb0> as Bitfield>::store_le`][msl] for more detailed information
on the memory views!

You can view the mask of all *storage regions* of a bit-slice by using its
[`.domain()`] method to view the breakdown of its memory region, then print the
[`.mask()`] of any [`PartialElement`] the domain contains. Whole elements are
always used in their entirety. You should use the `domain` module’s types
whenever you are uncertain of the exact locations in memory that a particular
bit-slice governs.

[lsl]: https://docs.rs/bitvec/latest/bitvec/field/trait.BitField.html#method.store_le-3
[msl]: https://docs.rs/bitvec/latest/bitvec/field/trait.BitField.html#method.store_le-4
[`PartialElement`]: crate::domain::PartialElement
[`.domain()`]: crate::slice::BitSlice::domain
[`.mask()`]: crate::domain::PartialElement::mask
