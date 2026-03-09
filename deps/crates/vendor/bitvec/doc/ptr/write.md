# Single-Bit Write

This writes a bit into `dst` directly.

## Original

[`ptr::write`](core::ptr::write)

## Safety

Because this performs a dereference of memory, it inherits the original
`ptr::write`’s requirements:

- `dst` must be valid to write
- `dst` must be properly aligned. This is an invariant of the `BitPtr` type as
  well as of the memory access.

Additionally, `dst` must point to an initialized value of `T`. Integers cannot
be initialized one bit at a time.

## Behavior

This is required to perform a read/modify/write cycle on the memory location.
LLVM *may or may not* emit a bit-write instruction on targets that have them in
the ISA, but this is not specified in any way.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let mut data = 0u8;
let ptr = BitPtr::<_, _, Lsb0>::from_mut(&mut data);
unsafe { bv_ptr::write(ptr.add(2), true); }
assert_eq!(data, 4);
```
