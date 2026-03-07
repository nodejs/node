# Shared Bit-Slice Splitting

This iterator yields `n` successive non-overlapping segments of a bit-slice,
separated by bits that match a predicate function. Splitting advances one
segment at a time, starting at the beginning of the bit-slice.

The matched bit is **not** included in the yielded segment. The `n`th yielded
segment does not attempt any further splits, and extends to the end of the
bit-slice.

It is created by the [`BitSlice::splitn`] method.

## Original

[`slice::SplitN`](core::slice::SplitN)

## API Differences

The predicate function receives both the index within the bit-slice, as well as
the bit value, in order to allow the predicate to have more than one bit of
information when splitting.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 0, 0, 1, 1, 1, 0, 1];
let mut split = bits.splitn(2, |idx, _bit| idx % 3 == 2);

assert_eq!(split.next().unwrap(), bits![0; 2]);
assert_eq!(split.next().unwrap(), bits![1, 1, 1, 0, 1]);
assert!(split.next().is_none());
```

[`BitSlice::splitn`]: crate::slice::BitSlice::splitn
