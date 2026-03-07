# Bit View

This trait describes a region of memory that can be viewed as its constituent
bits. It is blanket-implemented on all [`BitStore`] implementors, as well as
slices and arrays of them. It should not be implemented on any other types.

The contained extension methods allow existing memory to be easily viewd as
[`BitSlice`]s using dot-call method syntax rather than the more cumbersome
constructor functions in `BitSlice`’s inherent API.

Since the element type is already known to the implementor, the only type
parameter you need to provide when calling these methods is the bit-ordering.

## Examples

```rust
use bitvec::prelude::*;

let a = 0u16;
let a_bits: &BitSlice<u16, Lsb0> = a.view_bits::<Lsb0>();

let mut b = [0u8; 4];
let b_bits: &mut BitSlice<u8, Msb0> = b.view_bits_mut::<Msb0>();
```

[`BitSlice`]: crate::slice::BitSlice
[`BitStore`]: crate::store::BitStore
