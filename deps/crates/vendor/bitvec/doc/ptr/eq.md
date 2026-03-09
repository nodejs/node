# Bit-Pointer Equality

This compares two bit-pointers for equality by their address value, not by the
value of their referent bit. This does not dereference either.

## Original

[`ptr::eq`](core::ptr::eq)

## API Differences

The two bit-pointers can differ in their storage type parameters. `bitvec`
defines pointer equality only between pointers with the same underlying
[`BitStore::Mem`][0] element type. Numerically-equal bit-pointers with different
integer types *will not* compare equal, though this function will compile and
accept them.

This cannot compare encoded span poiters. `*const BitSlice` can be used in the
standard-library `ptr::eq`, and does not need an override.

## Examples

```rust
use bitvec::prelude::*;
use bitvec::ptr as bv_ptr;
use core::cell::Cell;

let data = 0u16;
let bare_ptr = BitPtr::<_, _, Lsb0>::from_ref(&data);
let cell_ptr = bare_ptr.cast::<Cell<u16>>();

assert!(bv_ptr::eq(bare_ptr, cell_ptr));

let byte_ptr = bare_ptr.cast::<u8>();
assert!(!bv_ptr::eq(bare_ptr, byte_ptr));
```

[0]: crate::store::BitStore::Mem
