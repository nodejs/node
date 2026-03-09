# Single-Bit Replacement

This writes a new value into a location, and returns the bit-value previously
stored there. It is semantically and behaviorally equivalent to
[`BitRef::replace`][0], except that it works on bit-pointer structures rather
than proxy references. Prefer to use a proxy reference or
[`BitSlice::replace`][1] instead.

## Original

[`ptr::replace`](core::ptr::replace)

## Safety

This has the same safety requirements as [`ptr::read`][2] and [`ptr::write`][3],
as it is required to use them in its implementation.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let mut data = 4u8;
let ptr = BitPtr::<_, _, Lsb0>::from_mut(&mut data);
assert!(unsafe {
  bv_ptr::replace(ptr.add(2), false)
});
assert_eq!(data, 0);
```

[0]: crate::ptr::BitRef::replace
[1]: crate::slice::BitSlice::replace
[2]: crate::ptr::read
[3]: crate::ptr::write
