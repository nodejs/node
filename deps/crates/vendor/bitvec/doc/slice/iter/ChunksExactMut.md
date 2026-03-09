# Exclusive Bit-Slice Exact Chunking

This iterator yields successive non-overlapping mutable chunks of a bit-slice.
Chunking advances one sub-slice at a time, starting at the beginning of the
bit-slice.

If the original bit-slice’s length is not evenly divided by the chunk width,
then the leftover segment at the back is not iterated, but can be accessed with
the [`.into_remainder()`] or [`.take_remainder()`] methods.

It is created by the [`BitSlice::chunks_exact_mut`] method.

## Original

[`slice::ChunksExactMut`](core::slice::ChunksExactMut)

## API Differences

This iterator marks all yielded bit-slices as `::Alias`ed.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![mut 0, 0, 0, 1, 1, 1, 0, 1];
let mut chunks = unsafe {
  bits.chunks_exact_mut(3).remove_alias()
};

chunks.next().unwrap().fill(true);
chunks.next().unwrap().fill(false);
assert!(chunks.next().is_none());
chunks.take_remainder().copy_from_bitslice(bits![1, 0]);
assert!(chunks.take_remainder().is_empty());

assert_eq!(bits, bits![1, 1, 1, 0, 0, 0, 1, 0]);
```

[`BitSlice::chunks_exact_mut`]: crate::slice::BitSlice::chunks_exact_mut
[`.into_remainder()`]: Self::into_remainder
[`.take_remainder()`]: Self::take_remainder
