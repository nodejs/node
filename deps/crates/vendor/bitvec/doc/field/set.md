# Partial-Element Setter

This function inserts a portion of an integer value into a [`PartialElement`].
The `BitField` implementations call it as they disassemble a complete integer.
It performs the following steps:

1. the value is `resize`d into a `T::Mem`,
1. shifted up from LSedge as needed to fit in the governed region of the partial
   element,
1. and then stored (after masking away excess bits) through the `PartialElement`
   into memory.

## Type Parameters

- `O` and `T` are the type parameters of the `PartialElement` argument.
- `U` is the source integer type.

## Parameters

- `elem`: A `PartialElement` into which a value segment will be written.
- `value`: A value, whose least-significant bits will be written into `elem`.
- `shamt`: The shift distance from the storage location’s LSedge to its live
  bits.

[`PartialElement`]: crate::domain::PartialElement
