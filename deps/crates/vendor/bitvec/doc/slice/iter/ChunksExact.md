# Shared Bit-Slice Exact Chunking

This iterator yields successive non-overlapping chunks of a bit-slice. Chunking
advances one sub-slice at a time, starting at the beginning of the bit-slice.

If the original bit-slice’s length is not evenly divided by the chunk width,
then the leftover segment at the back is not iterated, but can be accessed with
the [`.remainder()`] method.

It is created by the [`BitSlice::chunks_exact`] method.

## Original

[`slice::ChunksExact`](core::slice::ChunksExact)

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 0, 0, 1, 1, 1, 0, 1];
let mut chunks = bits.chunks_exact(3);

assert_eq!(chunks.next().unwrap(), bits![0; 3]);
assert_eq!(chunks.next().unwrap(), bits![1; 3]);
assert!(chunks.next().is_none());
assert_eq!(chunks.remainder(), bits![0, 1]);
```

[`BitSlice::chunks_exact`]: crate::slice::BitSlice::chunks_exact
[`.remainder()`]: Self::remainder
