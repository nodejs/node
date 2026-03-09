# Bit-Vector Extension by Proxy References

**DO NOT** use this. You *clearly* have a bit-slice. Use
[`.extend_from_bitslice()`] instead!

Iterating over a bit-slice requires loading from memory and constructing a proxy
reference for each bit. This is needlessly slow; the specialized method is able
to avoid this per-bit cost and possibly even use batched operations.

[`.extend_from_bitslice()`]: crate::vec::BitVec::extend_from_bitslice
