# Bit-Pointer Hashing

This hashes a bit-pointer by the value of its components, rather than its
referent bit. It does not dereference the pointer.

This can be used to ensure that you are hashing the bit-pointer’s address value,
though, as always, hashing an address rather than a data value is likely unwise.

## Original

[`ptr::hash`](core::ptr::hash)
