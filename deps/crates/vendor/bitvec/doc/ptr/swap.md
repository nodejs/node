# Bit Swap

This exchanges the bit-values in two locations. It is semantically and
behaviorally equivalent to [`BitRef::swap`][0], except that it works on
bit-pointer structures rather than proxy references. Prefer to use a proxy
reference or [`BitSlice::swap`][1] instead.

## Original

[`ptr::swap`](core::ptr::swap)

## Safety

This has the same safety requirements as [`ptr::read`][2] and [`ptr::write`][3],
as it is required to use them in its implementation.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let mut data = 2u8;
let x = BitPtr::<_, _, Lsb0>::from_mut(&mut data);
let y = unsafe { x.add(1) };

unsafe { bv_ptr::swap(x, y); }
assert_eq!(data, 1);
```

[0]: crate::ptr::BitRef::swap
[1]: crate::slice::BitSlice::swap
[2]: crate::ptr::read
[3]: crate::ptr::write
