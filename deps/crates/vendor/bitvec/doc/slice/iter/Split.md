# Shared Bit-Slice Splitting

This iterator yields successive non-overlapping segments of a bit-slice,
separated by bits that match a predicate function. Splitting advances one
segment at a time, starting at the beginning of the bit-slice.

The matched bit is **not** included in the yielded segment.

It is created by the [`BitSlice::split`] method.

## Original

[`slice::Split`](core::slice::Split)

## API Differences

The predicate function receives both the index within the bit-slice, as well as
the bit value, in order to allow the predicate to have more than one bit of
information when splitting.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 0, 0, 1, 1, 1, 0, 1];
let mut split = bits.split(|idx, _bit| idx % 3 == 2);

assert_eq!(split.next().unwrap(), bits![0; 2]);
assert_eq!(split.next().unwrap(), bits![1; 2]);
assert_eq!(split.next().unwrap(), bits![0, 1]);
assert!(split.next().is_none());
```

[`BitSlice::split`]: crate::slice::BitSlice::split
