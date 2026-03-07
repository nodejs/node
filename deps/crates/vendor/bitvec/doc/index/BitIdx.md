# Semantic Bit Index

This type is a counter in the ring `0 .. R::BITS` and serves to mark a semantic
index within some register element. It is a virtual index, and is the stored
value used in pointer encodings to track region start information.

It is translated to a real index through the [`BitOrder`] trait. This virtual
index is the only counter that can be used for address computation, and once
lowered to an electrical index through [`BitOrder::at`], the electrical address
can only be used for setting up machine instructions.

## Type Parameters

- `R`: The register element that this index governs.

## Validity

Values of this type are **required** to be in the range `0 .. R::BITS`. Any
value not less than [`R::BITS`] makes the program invalid, and will likely cause
either a crash or incorrect memory access.

## Construction

This type can never be constructed outside of the `bitvec` crate. It is passed
in to [`BitOrder`] implementations, which may use it to construct electrical
position values from it. All values of this type constructed by `bitvec` are
known to be correct in their region; no other construction site can be trusted.

[`BitOrder`]: crate::order::BitOrder
[`BitOrder::at`]: crate::order::BitOrder::at
[`R::BITS`]: funty::Integral::BITS
