# Exclusive Bit-Slice Chunking

This iterator yields successive non-overlapping mutable chunks of a bit-slice.
Chunking advances one subslice at a time, starting at the end of the bit-slice.

If the original bit-slice’s length is not evenly divided by the chunk width,
then the final chunk will be the remainder, and will be shorter than requested.

It is created by the [`BitSlice::chunks_mut`] method.

## Original

[`slice::ChunksMut`](core::slice::ChunksMut)

## API Differences

This iterator marks all yielded bit-slices as `::Alias`ed.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![mut 0, 1, 0, 0, 0, 1, 1, 1];
let mut chunks = unsafe {
  bits.rchunks_mut(3).remove_alias()
};

chunks.next().unwrap().fill(false);
chunks.next().unwrap().fill(true);
chunks.next().unwrap().copy_from_bitslice(bits![1, 0]);
assert!(chunks.next().is_none());

assert_eq!(bits, bits![1, 0, 1, 1, 1, 0, 0, 0]);
```

[`BitSlice::chunks_mut`]: crate::slice::BitSlice::chunks_mut
