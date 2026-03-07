# Shared Bit-Slice Iteration

This view iterates each bit in the bit-slice by [proxy reference][0]. It is
created by the [`BitSlice::iter`] method.

## Original

[`slice::Iter`](core::slice::Iter)

## API Differences

While this iterator can manifest `&bool` references, it instead yields the
`bitvec` [proxy reference][0] for consistency with the [`IterMut`] type. It can
be converted to yield true references with [`.by_refs()`]. Additionally, because
it does not yield `&bool`, the [`Iterator::copied`] method does not apply. It
can be converted to an iterator of `bool` values with [`.by_vals()`].

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![0, 1];
for bit in bits.iter() {
  # #[cfg(feature = "std")] {
  println!("{}", bit);
  # }
}
```

[`BitSlice::iter`]: crate::slice::BitSlice::iter
[`IterMut`]: crate::slice::IterMut
[`Iterator::copied`]: core::iter::Iterator::copied
[`.by_refs()`]: Self::by_refs
[`.by_vals()`]: Self::by_vals
[0]: crate::ptr::BitRef
