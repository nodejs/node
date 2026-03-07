# Splicing Iteration

This adapts a [`Drain`] to overwrite the drained section with the contents of
another iterator.

When this splice is destroyed, the drained section of the source bit-vector is
replaced with the contents of the replacement iterator. If the replacement is
not the same length as the drained section, then the bit-vector is resized to
fit.

See [`BitVec::splice()`] for more information.

## Original

[`vec::Splice`](alloc::vec::Splice)

[`BitVec::splice()`]: crate::vec::BitVec::splice
