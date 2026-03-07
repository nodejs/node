# Bit-Slice to Bit-Array Conversion Error

This error is produced when an `&BitSlice` view is unable to be recast as a
`&BitArray` view with the same parameters.

Unlike ordinary scalars and arrays, where arrays are never aligned more
stringently than their components, `BitSlice` is aligned to an individual bit
while `BitArray` is aligned to its `A` storage type.

This is produced whenever a `&BitSlice` view is not exactly as long as the
destination `&BitArray` view is, or does not also begin at the zeroth bit in an
`A::Store` element.

## Original

[`array::TryFromSliceError`](core::array::TryFromSliceError)
