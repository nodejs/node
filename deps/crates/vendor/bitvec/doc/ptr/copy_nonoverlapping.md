# Bit-wise `memcpy`

This copies bits from a region beginning at `src` into a region beginning at
`dst`, each extending upwards in the address space for `count` bits.

The two regions *may not* overlap.

## Original

[`ptr::copy_nonoverlapping`](core::ptr::copy_nonoverlapping)

## Overlap Definition

The two regions may be in the same provenance as long as they have no common
bits. `bitvec` only defines the possibility of overlap when the `O1` and `O2`
bit-ordering parameters are the same; if they are different, then it considers
the regions to not overlap, and does not attempt to detect real-memory
collisions.

## Safety

In addition to the bit-ordering constraints, this inherits the restrictions of
the original `ptr::copy_nonoverlapping`:

- `src` must be valid to read the next `count` bits out of memory.
- `dst` must be valid to write into the next `count` bits.
- Both `src` and `dst` must satisfy [`BitPtr`]’s non-null, well-aligned,
  requirements.

## Behavior

This reads and writes each bit individually. It is incapable of optimizing its
behavior to perform batched memory accesses that have better awareness of the
underlying memory.

The [`BitSlice::copy_from_bitslice`][1] method *is* able to perform this
optimization, and tolerates overlap. You should always prefer to use `BitSlice`
if you are sensitive to performance.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let start = 0b1011u8;
let mut end = 0u16;

let src = BitPtr::<_, _, Lsb0>::from_ref(&start);
let dst = BitPtr::<_, _, Msb0>::from_mut(&mut end);

unsafe {
  bv_ptr::copy_nonoverlapping(src, dst, 4);
}
assert_eq!(end, 0b1101_0000_0000_0000);
```

[1]: crate::slice::BitSlice::copy_from_bitslice
[`BitPtr`]: crate::ptr::BitPtr
