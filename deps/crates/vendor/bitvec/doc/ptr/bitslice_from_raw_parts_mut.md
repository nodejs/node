# Bit-Slice Pointer Construction

This forms a raw [`BitSlice`] pointer from a bit-pointer and a length.

## Original

[`ptr::slice_from_raw_parts`](core::ptr::slice_from_raw_parts)

## Examples

You will need to construct a `BitPtr` first; these are typically produced by
existing `BitSlice` views, or you can do so manually.

```rust
use bitvec::{
  prelude::*,
  index::BitIdx,
  ptr as bv_ptr,
};

let mut data = 6u16;
let head = BitIdx::new(1).unwrap();
let ptr = BitPtr::<_, _, Lsb0>::new((&mut data).into(), head).unwrap();
let slice = bv_ptr::bitslice_from_raw_parts_mut(ptr, 10);
let slice_ref = unsafe { &mut *slice };
assert_eq!(slice_ref.len(), 10);
slice_ref.set(2, true);
assert_eq!(slice_ref, bits![1, 1, 1, 0, 0, 0, 0, 0, 0, 0]);
```

[`BitSlice`]: crate::slice::BitSlice
