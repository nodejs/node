# Exclusive Bit-Slice Reverse Splitting

This iterator yields `n` successive non-overlapping mutable segments of a
bit-slice, separated by bits that match a predicate function. Splitting advances
one segment at a time, starting at the end of the bit-slice.

The matched bit is **not** included in the yielded segment. The `n`th yielded
segment does not attempt any further splits, and extends to the front of the
bit-slice.

It is created by the [`BitSlice::rsplitn_mut`] method.

## Original

[`slice::SplitNMut`](core::slice::SplitNMut)

## API Differences

This iterator marks all yielded bit-slices as `::Alias`ed.

The predicate function receives both the index within the bit-slice, as well as
the bit value, in order to allow the predicate to have more than one bit of
information when splitting.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![mut 0, 0, 0, 1, 1, 1, 0, 1];
let mut split = bits.rsplitn_mut(2, |idx, _bit| idx % 3 == 2);

split.next().unwrap().fill(false);
split.next().unwrap().fill(false);
assert!(split.next().is_none());

assert_eq!(bits, bits![0, 0, 0, 0, 0, 1, 0, 0]);
```

[`BitSlice::rsplitn_mut`]: crate::slice::BitSlice::rsplitn_mut
