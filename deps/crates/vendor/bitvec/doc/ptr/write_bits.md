# Bit-wise `memset`

This fills a region of memory with a bit value. It is equivalent to using
`memset` with only `!0` or `0`, masked appropriately for the region edges.

## Original

[`ptr::write_bytes`](core::ptr::write_bytes)

## Safety

Because this performs a dereference of memory, it inherits the original
`ptr::write_bytes`’ requirements:

- `dst` must be valid to write
- `dst` must be properly aligned. This is an invariant of the `BitPtr` type as
  well as of the memory access.

Additionally, `dst` must point to an initialized value of `T`. Integers cannot
be initialized one bit at a time.

## Behavior

This function does not specify an implementation. You should assume the worst
case (`O(n)` read/modify/write of each bit). The [`BitSlice::fill`][0] method
will have equal or better performance.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let mut data = 0u8;
let ptr = BitPtr::<_, _, Lsb0>::from_mut(&mut data);
unsafe {
  bv_ptr::write_bits(ptr.add(1), true, 5);
}
assert_eq!(data, 0b0011_1110);
```

[0]: crate::slice::BitSlice::fill
