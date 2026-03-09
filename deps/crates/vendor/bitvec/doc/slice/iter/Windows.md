# Bit-Slice Windowing

This iterator yields successive overlapping windows into a bit-slice. Windowing
advances one bit at a time, so for any given window width `N`, most bits will
appear in `N` windows. Windows do not “extend” past either edge of the
bit-slice: the first window has its front edge at the front of the bit-slice,
and the last window has its back edge at the back of the bit-slice.

It is created by the [`BitSlice::windows`] method.

## Original

[`slice::Windows`](core::slice::Windows)

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 0, 1, 1, 0];
let mut windows = bits.windows(2);
let expected = &[
  bits![0, 0],
  bits![0, 1],
  bits![1, 1],
  bits![1, 0],
];

assert_eq!(windows.len(), 4);
for (window, expected) in windows.zip(expected) {
  assert_eq!(window, expected);
}
```

[`BitSlice::windows`]: crate::slice::BitSlice::windows
