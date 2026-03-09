# Exclusive Bit-Slice Iteration

This view iterates each bit in the bit-slice by exclusive proxy reference. It is
created by the [`BitSlice::iter_mut`] method.

## Original

[`slice::IterMut`](core::slice::IterMut)

## API Differences

Because `bitvec` cannot manifest `&mut bool` references, this instead yields the
crate [proxy reference][0]. Because the proxy is a true type, rather than an
`&mut` reference, its name must be bound with `mut` in order to write through
it.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![mut 0, 1];
for mut bit in bits.iter_mut() {
  *bit = !*bit;
}
assert_eq!(bits, bits![1, 0]);
```

[`BitSlice::iter_mut`]: crate::slice::BitSlice::iter_mut
[0]: crate::ptr::BitRef
