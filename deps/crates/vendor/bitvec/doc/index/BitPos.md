# Bit Position

This is a position counter of a real bit in an `R` memory element.

Like [`BitIdx`], it is a counter in the ring `0 .. R::BITS`. It marks a real bit
in memory, and is the shift distance in the expression `1 << n`. It can only be
produced by applying [`BitOrder::at`] to an existing `BitIdx` produced by
`bitvec`.

## Type Parameters

- `R`: The register element that this position governs.

## Validity

Values of this type are **required** to be in the range `0 .. R::BITS`. Any
value not less than [`R::BITS`] makes the program invalid, and will likely cause
either a crash or incorrect memory access.

## Construction

This type is publicly constructible, but is only correct to do so within an
implementation of `BitOrder::at`. `bitvec` will only request its creation
through that trait implementation, and has no sites that can publicly accept
untrusted values.

[`BitIdx`]: crate::index::BitIdx
[`BitOrder::at`]: crate::order::BitOrder::at
[`R::BITS`]: funty::Integral::BITS
