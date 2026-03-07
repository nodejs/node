# Bit-Array De/Serialization

The Serde model distinguishes between *sequences*, which have a dynamic length
which must always be transported with the data, and *tuples*, which have a fixed
length known at compile-time that does not require transport.

Serde handles arrays using its tuple model, not its sequence model, which means
that `BitArray` cannot use the `BitSlice` Serde implementations (which must use
the sequence model in order to handle `&[u8]` and `Vec<T>` de/serialization).
Instead, `BitArray` has a standalone implementation using the tuple model so
that its wrapped array can be transported (nearly) as if it were unwrapped.

For consistency, `BitArray` has the same wire format that `BitSlice` does; the
only distinction is that the data buffer is a tuple rather than a sequence.

Additionally, Serde’s support for old versions of Rust means that it only
implements its traits on arrays `[T; 0 ..= 32]`. Since `bitvec` has a much
higher MSRV that includes support for the const-generic `[T; N]` family, it
reïmplements Serde’s behavior on a custom `Array<T, N>` type in order to ensure
that all possible `BitArray` storage types are transportable. Note, however,
that *because* each `[T; N]` combination is a new implementation, de/serializing
`BitArray`s directly is a great way to pessimize codegen.

While it would be nice if `rustc` or LLVM could collapse the implementations and
restore `N` as a run-time argument rather than a compile-time constant, neither
`bitvec` nor Serde attempt to promise this in any way. Use at your discretion.
