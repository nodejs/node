# Proxy Bit-References

Rust does not permit the use of custom proxy structures in place of true
reference primitives, so APIs that specify references (like `IndexMut` or
`DerefMut`) cannot be implemented by types that cannot manifest `&mut`
references directly. Since `bitvec` cannot produce an `&mut bool` reference
within a `BitSlice`, it instead uses the `BitRef` proxy type defined in this
module to provide reference-like work generally, and simply does not define
`IndexMut<usize>`.
