# Exclusive Bit-Slice Splitting

This iterator yields successive non-overlapping mutable segments of a bit-slice,
separated by bits that match a predicate function. Splitting advances one
segment at a time, starting at the beginning of the bit-slice.

The matched bit **is** included in the yielded segment.

It is created by the [`BitSlice::split_inclusive_mut`] method.

## Original

[`slice::SplitInclusiveMut`](core::slice::SplitInclusiveMut)

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![mut 0, 0, 0, 1, 1, 1, 0, 1];
let mut split = unsafe {
  bits.split_inclusive_mut(|idx, _bit| idx % 3 == 2).remove_alias()
};

split.next().unwrap().fill(true);
split.next().unwrap().fill(false);
split.next().unwrap().copy_from_bitslice(bits![1, 0]);
assert!(split.next().is_none());

assert_eq!(bits, bits![1, 1, 1, 0, 0, 0, 1, 0]);
```

[`BitSlice::split_inclusive_mut`]: crate::slice::BitSlice::split_inclusive_mut
