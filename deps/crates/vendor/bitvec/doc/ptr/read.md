# Single-Bit Read

This reads the bit out of `src` directly.

## Original

[`ptr::read`](core::ptr::read)

## Safety

Because this performs a dereference of memory, it inherits the original
`ptr::read`’s requirements:

- `src` must be valid to read.
- `src` must be properly aligned. This is an invariant of the `BitPtr` type as
  well as of the memory access.
- `src` must point to an initialized value of `T`.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let data = 128u8;
let ptr = BitPtr::<_, _, Msb0>::from_ref(&data);
assert!(unsafe { bv_ptr::read(ptr) });
```
