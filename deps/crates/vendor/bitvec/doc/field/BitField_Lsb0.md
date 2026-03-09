# `Lsb0` Bit-Field Behavior

`BitField` has no requirements about the in-memory representation or layout of
stored integers within a bit-slice, only that round-tripping an integer through
a store and a load of the same element suffix on the same bit-slice is
idempotent (with respect to sign truncation).

`Lsb0` provides a contiguous translation from bit-index to real memory: for any
given bit index `n` and its position `P(n)`, `P(n + 1)` is `P(n) + 1`. This
allows it to provide batched behavior: since the section of contiguous indices
used within an element translates to a section of contiguous bits in real
memory, the transaction is always a single shift/mask operation.

Each implemented method contains documentation and examples showing exactly how
the abstract integer space is mapped to real memory.
