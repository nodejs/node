# Partial-Element Getter

This function extracts a portion of an integer value from a [`PartialElement`].
The `BitField` implementations call it as they assemble a complete integer. It
performs the following steps:

1. the `PartialElement` is loaded (and masked to discard unused bits),
1. the loaded value is then shifted to abut the LSedge of the stack local,
1. and then `resize`d into a `U` value.

## Type Parameters

- `O` and `T` are the type parameters of the `PartialElement` argument.
- `U` is the destination integer type.

## Parameters

- `elem`: A `PartialElement` containing a value segment.
- `shamt`: The distance by which to right-shift the value loaded from `elem` so
  that it abuts the LSedge.

## Returns

The segment of an integer stored in `elem`.

[`PartialElement`]: crate::domain::PartialElement
