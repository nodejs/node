# Bit-Vector Iteration

Bit-vectors have the advantage that iteration consumes the whole structure, so
they can simply freeze the allocation into a bit-box, then use its iteration and
destructor.

## Original

[`impl<T> IntoIterator for Vec<T>`][orig]

[orig]: https://doc.rust-lang.org/alloc/vec/struct.Vec.html#impl-IntoIterator
