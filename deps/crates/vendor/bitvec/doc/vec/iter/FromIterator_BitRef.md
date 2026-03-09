# Bit-Vector Collection from Proxy References

**DO NOT** use this. You *clearly* have a bit-slice. Use
[`::from_bitslice()`] instead!

Iterating over a bit-slice requires loading from memory and constructing a proxy
reference for each bit. This is needlessly slow; the specialized method is able
to avoid this per-bit cost and possibly even use batched operations.

[`::from_bitslice()`]: crate::vec::BitVec::from_bitslice
