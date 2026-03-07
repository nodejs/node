# Bit-wise `memcpy`

This copies bits from a region beginning at `src` into a region beginning at
`dst`, each extending upwards in the address space for `count` bits.

The two regions may overlap.

If the two regions are known to *never* overlap, then [`copy_nonoverlapping`][0]
can be used instead.

## Original

[`ptr::copy`](core::ptr::copy)

## Overlap Definition

`bitvec` defines region overlap only when the bit-pointers used to access them
have the same `O: BitOrder` type parameter. When this parameter differs, the
regions are always assumed to not overlap in real memory, because `bitvec` does
not define the effects of different orderings mapping to the same locations.

## Safety

In addition to the bit-ordering constraints, this inherits the restrictions of
the original `ptr::copy`:

- `src` must be valid to read the next `count` bits out of memory.
- `dst` must be valid to write into the next `count` bits.
- Both `src` and `dst` must satisfy [`BitPtr`]’s non-null, well-aligned,
  requirements.

## Behavior

This reads and writes each bit individually. It is incapable of optimizing its
behavior to perform batched memory accesses that have better awareness of the
underlying memory.

The [`BitSlice::copy_from_bitslice`][1] method *is* able to perform this
optimization. You should always prefer to use `BitSlice` if you are sensitive to
performance.

## Examples

This example performs a simple copy across independent regions. You can see that
it follows the ordering parameter for the source and destination regions as it
walks each bit individually.

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let start = 0b1011u8;
let mut end = 0u16;

let src = BitPtr::<_, _, Lsb0>::from_ref(&start);
let dst = BitPtr::<_, _, Msb0>::from_mut(&mut end);

unsafe {
  bv_ptr::copy(src, dst, 4);
}
assert_eq!(end, 0b1101_0000_0000_0000);
```

This can detect overlapping regions. Note again that overlap only exists when
the ordering parameter is the same! Using bit-pointers that overlap in real
memory with different ordering is not defined, and `bitvec` does not specify any
result.

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let mut x = 0b1111_0010u8;
let src = BitPtr::<_, _, Lsb0>::from_mut(&mut x);
let dst = unsafe { src.add(2) };

unsafe {
  bv_ptr::copy(src.to_const(), dst, 4);
}

assert_eq!(x, 0b1100_1010);
// bottom nibble  ^^ ^^ moved here
```

[`BitPtr`]: crate::ptr::BitPtr
[0]: crate::ptr::copy_nonoverlapping
[1]: crate::slice::BitSlice::copy_from_bitslice
