# Single-Bit Unaligned Read

This reads the bit out of `src` directly. It uses compiler intrinsics to
tolerate an unaligned `T` address. However, because `BitPtr` has a type
invariant that addresses are always well-aligned (and non-null), this has no
benefit or purpose.

## Original

[`ptr::read_unaligned`](core::ptr::read_unaligned)

## Safety

Because this performs a dereference of memory, it inherits the original
`ptr::read_unaligned`’s requirements:

- `src` must be valid to read.
- `src` must point to an initialized value of `T`.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;


let data = 128u8;
let ptr = BitPtr::<_, _, Msb0>::from_ref(&data);
assert!(unsafe { bv_ptr::read_unaligned(ptr) });
```
