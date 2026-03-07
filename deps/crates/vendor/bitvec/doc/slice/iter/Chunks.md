# Shared Bit-Slice Chunking

This iterator yields successive non-overlapping chunks of a bit-slice. Chunking
advances one subslice at a time, starting at the beginning of the bit-slice.

If the original bit-slice’s length is not evenly divided by the chunk width,
then the final chunk will be the remainder, and will be shorter than requested.

It is created by the [`BitSlice::chunks`] method.

## Original

[`slice::Chunks`](core::slice::Chunks)

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 0, 0, 1, 1, 1, 0, 1];
let mut chunks = bits.chunks(3);

assert_eq!(chunks.next().unwrap(), bits![0; 3]);
assert_eq!(chunks.next().unwrap(), bits![1; 3]);
assert_eq!(chunks.next().unwrap(), bits![0, 1]);
assert!(chunks.next().is_none());
```

[`BitSlice::chunks`]: crate::slice::BitSlice::chunks
