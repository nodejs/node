# Single-Bit Volatile Read

This reads the bit out of `src` directly, using a volatile I/O intrinsic to
prevent compiler reördering or removal.

You should not use `bitvec` to perform any volatile I/O operations. You should
instead do volatile I/O work on integer values directly, or use a crate like
[`voladdress`][0] to perform I/O transactions, and use `bitvec` only on stack
locals that have no additional memory semantics.

## Original

[`ptr::read_volatile`](core::ptr::read_volatile)

## Safety

Because this performs a dereference of memory, it inherits the original
`ptr::read_volatile`’s requirements:

- `src` must be valid to read.
- `src` must be properly aligned. This is an invariant of the `BitPtr` type as
  well as of the memory access.
- `src` must point to an initialized value of `T`.

Remember that volatile accesses are ordinary loads that the compiler cannot
remove or reörder! They are *not* an atomic synchronizer.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;

let data = 128u8;
let ptr = BitPtr::<_, _, Msb0>::from_ref(&data);
assert!(unsafe { bv_ptr::read_volatile(ptr) });
```

[0]: https://docs.rs/voladdress/latest/voladdress
