# Sign Extension

When a bit-slice loads a value whose destination type is wider than the
bit-slice itself, and the destination type is a signed integer, the loaded value
must be sign-extended. The load accumulator always begins as the zero pattern,
and the loaders do not attempt to detect a sign bit before they begin.

As such, this function takes a value loaded out of a bit-slice, which has been
zero-extended from the storage length to the destination type, and the length of
the bit-slice that contained it. If the destination type is unsigned, then the
value is returned as-is; if the destination type is signed, then the value is
sign-extended according to the bit at `1 << (width - 1)`.

## Type Parameters

- `I`: The integer type of the loaded element. When this is one of
  `u{8,16,32,64,size}`, no sign extension takes place.

## Parameters

- `elem`: The value loaded out of a bit-slice.
- `width`: The width in bits of the source bit-slice. This is always known to be
  in the domain `1 ..= I::BITS`.

## Returns

A correctly-signed copy of `elem`. Unsigned integers, and signed integers whose
most significant loaded bit was `0`, are untouched. Signed integers whose most
significant loaded bit was `1` have their remaining high bits set to `1` for
sign extension.
