# Reading From a Bit-Vector

The implementation loads bytes out of the reference bit-vector until either the
destination buffer is filled or the source has no more bytes to provide. When
`.read()` returns, the provided bit-vector will have its contents shifted down
so that it begins at the first bit *after* the last byte copied out into `buf`.

Note that the return value of `.read()` is always the number of *bytes* of `buf`
filled!

## API Differences

The standard library does not `impl Read for Vec<u8>`. It is provided here as a
courtesy.
