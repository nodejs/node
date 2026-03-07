# Bit-Array Implementation of `BitField`

The `BitArray` implementation is only ever called when the entire bit-array is
available for use, which means it can skip the bit-slice memory detection and
instead use the underlying storage elements directly.

The implementation still performs the segmentation for each element contained in
the array, in order to maintain value consistency so that viewing the array as a
bit-slice is still able to correctly interact with data contained in it.
