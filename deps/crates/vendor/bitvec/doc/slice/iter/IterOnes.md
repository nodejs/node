# Bit Seeking

This iterator yields indices of bits set to `1`, rather than bit-values
themselves. It is essentially the inverse of indexing: rather than applying a
`usize` to the bit-slice to get a `bool`, this applies a `bool` to get a
`usize`.

It is created by the [`.iter_ones()`] method on bit-slices.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 1, 0, 0, 1];
let mut ones = bits.iter_ones();

assert_eq!(ones.next(), Some(1));
assert_eq!(ones.next(), Some(4));
assert!(ones.next().is_none());
```

[`.iter_ones()`]: crate::slice::BitSlice::iter_ones
