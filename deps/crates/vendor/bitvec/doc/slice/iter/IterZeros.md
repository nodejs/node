# Bit Seeking

This iterator yields indices of bits cleared to `0`, rather than bit-values
themselves. It is essentially the inverse of indexing: rather than applying a
`usize` to the bit-slice to get a `bool`, this applies a `bool` to get a
`usize`.

It is created by the [`.iter_zeros()`] method on bit-slices.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![1, 0, 1, 1, 0];
let mut zeros = bits.iter_zeros();

assert_eq!(zeros.next(), Some(1));
assert_eq!(zeros.next(), Some(4));
assert!(zeros.next().is_none());
```

[`.iter_zeros()`]: crate::slice::BitSlice::iter_zeros
