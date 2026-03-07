# Raw Bit-Slice Construction

This is equivalent to [`slice::from_raw_parts()`], except that it does not check
any of the encoding requirements.

## Safety

Callers must both uphold the safety requirements of that function, as well as
ensure that the arguments would not cause it to fail gracefully.

Arguments that would cause `from_raw_parts` to return `Err` instead produce a
bit-slice handle whose value is undefined.

## Parameters

1. `ptr`: A bit-pointer to a `T` memory element. The pointer’s data address must
   be well-aligned, the bit-index must be valid for `T`, the target region
   must be initialized for `len` bits.
1. `len`: A count of live bits beginning at `ptr`. It must not exceed
   [`MAX_BITS`].

## Returns

An exclusive `BitSlice` reference over the described region. If either of the
parameters are invalid, then the value of the reference is library-level
undefined.

[`MAX_BITS`]: crate::slice::BitSlice::MAX_BITS
[`slice::from_raw_parts()`]: crate::slice::from_raw_parts
