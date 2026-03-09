# Bit-Pointer Sentinel Value

`BitPtr` does not permit actual null pointers. Instead, it uses the canonical
dangling address as a sentinel for uninitialized, useless, locations.

You should use `Option<BitPtr>` if you need to track nullability.

## Original

[`ptr::null_mut`](core::ptr::null_mut)
