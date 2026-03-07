# Shared Bit-Slice Reverse Splitting

This iterator yields successive non-overlapping segments of a bit-slice,
separated by bits that match a predicate function. Splitting advances one
segment at a time, starting at the end of the bit-slice.

The matched bit is **not** included in the yielded segment.

It is created by the [`BitSlice::rsplit`] method.

## Original

[`slice::RSplit`](core::slice::RSplit)

## API Differences

The predicate function receives both the index within the bit-slice, as well as
the bit value, in order to allow the predicate to have more than one bit of
information when splitting.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 0, 0, 1, 1, 1, 0, 1];
let mut split = bits.rsplit(|idx, _bit| idx % 3 == 2);

assert_eq!(split.next().unwrap(), bits![0, 1]);
assert_eq!(split.next().unwrap(), bits![1; 2]);
assert_eq!(split.next().unwrap(), bits![0; 2]);
assert!(split.next().is_none());
```

[`BitSlice::rsplit`]: crate::slice::BitSlice::rsplit
