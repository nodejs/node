# Immutable Bit View

This trait is an analogue to the [`AsRef`] trait, in that it enables any type to
provide a view of an immutable bit-slice.

It does not require an `AsRef<[T: BitStore]>` implementation, but a blanket
implementation for all `AsRef<[T: BitStore]>` is provided. This allows you to
choose whether to implement only one of `AsBits<T>` or `AsRef<[T]>`, and gain
a bit-slice view through either choice.

## Usage

The `.as_bits<_>()` method has the same usage patterns as
[`BitView::view_bits`][0].

## Notes

You are not *forbidden* from creating multiple views with different element
types to the same region, but doing so is likely to cause inconsistent and
surprising behavior.

Refrain from implementing this trait with more than one storage argument unless
you are sure that you can uphold the memory region requirements of all of them,
and are aware of the behavior conflicts that may arise.

[0]: crate::view::BitView::view_bits
[`AsRef`]: core::convert::AsRef
