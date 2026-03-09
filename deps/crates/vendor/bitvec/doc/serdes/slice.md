# Bit-Slice De/Serialization

Bit-slice references and containers serialize as sequences with additional
metadata.

Serde only provides a deserializer for `&[u8]`; wider integers and
interior-mutability wrappers are not able to view a transport buffer without
potentially modifying it, and the buffer is not modifiable while being used for
deserialization. As such, only `&BitSlice<u8, O>` has a no-copy deserialization
implementation.

If you need other storage types, you will need to deserialize into a `BitBox` or
`BitVec`. If you do not have an allocator, you must *serialize from* and
deserialize into a `BitArray`.
