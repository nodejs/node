# Shared Bit-Slice Splitting

This iterator yields successive non-overlapping segments of a bit-slice,
separated by bits that match a predicate function. Splitting advances one
segment at a time, starting at the beginning of the bit-slice.

The matched bit **is** included in the yielded segment.

It is created by the [`BitSlice::split_inclusive`] method.

## Original

[`slice::SplitInclusive`](core::slice::SplitInclusive)

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 0, 0, 1, 1, 1, 0, 1];
let mut split = bits.split_inclusive(|idx, _bit| idx % 3 == 2);

assert_eq!(split.next().unwrap(), bits![0; 3]);
assert_eq!(split.next().unwrap(), bits![1; 3]);
assert_eq!(split.next().unwrap(), bits![0, 1]);
assert!(split.next().is_none());
```

[`BitSlice::split_inclusive`]: crate::slice::BitSlice::split_inclusive
