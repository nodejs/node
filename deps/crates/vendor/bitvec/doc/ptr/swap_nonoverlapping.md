# Many-Bit Swap

Exchanges the contents of two regions, which cannot overlap.

## Original

[`ptr::swap_nonoverlapping`](core::ptr::swap_nonoverlapping)

## Safety

Both `one` and `two` must be:

- correct `BitPtr` instances (well-aligned, non-null)
- valid to read and write for the next `count` bits

Additionally, the ranges `one .. one + count` and `two .. two + count` must be
entirely disjoint. They can be adjacent, but no bit can be in both.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let mut x = [0u8; 2];
let mut y = !0u16;
let x_ptr = BitPtr::<_, _, Msb0>::from_slice_mut(&mut x);
let y_ptr = BitPtr::<_, _, Lsb0>::from_mut(&mut y);

unsafe {
  bv_ptr::swap_nonoverlapping(x_ptr, y_ptr, 12);
}
assert_eq!(x, [!0, 0xF0]);
assert_eq!(y, 0xF0_00);
```
