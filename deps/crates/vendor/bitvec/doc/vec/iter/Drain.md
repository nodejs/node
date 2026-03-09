# Draining Iteration

This structure iterates over a subset of a bit-vector, yielding each bit and
removing it completely from the source.

Each drain locks the bit-vector that created it until the drain is either
destroyed or forgotten. If a drain is leaked rather than being allowed to drop
normally, the source bit-vector is only guaranteed to have contents up to the
original start of the drain. All further contents are unspecified.

See [`BitVec::drain()`] for more details.

## Original

[`vec::Drain`](alloc::vec::Drain)

[`BitVec::drain()`]: crate::vec::BitVec::drain
