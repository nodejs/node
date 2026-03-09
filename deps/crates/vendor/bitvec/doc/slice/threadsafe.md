# Bit-Slice Thread Safety

This allows bit-slice references to be moved across thread boundaries only when
the underlying `T` element can tolerate concurrency.

All `BitSlice` references, shared or exclusive, are only threadsafe if the `T`
element type is `Send`, because any given bit-slice reference may only have
partial control of a memory element that is also being shared by a bit-slice
reference on another thread. As such, this is never implemented for `Cell<U>`,
but always implemented for `AtomicU` and `U` for a given unsigned integer type
`U`.

Atomic integers safely handle concurrent writes, cells do not allow concurrency
at all, so the only missing piece is `&mut BitSlice<_, U: Unsigned>`. This is
handled by the aliasing system that the mutable splitters employ: a mutable
reference to an unsynchronized bit-slice can only cross threads when no other
handle is able to exist to the elements it governs. Splitting a mutable
bit-slice causes the split halves to change over to either atomics or cells, so
concurrency is either safe or impossible.
