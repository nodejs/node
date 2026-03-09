//! A bidirectional iterator that only checks its direction once.

use core::iter::FusedIterator;

/** An iterator that conditionally reverses itself upon creation.

This acts as a conditional `.rev()` adapter: it reverses the direction of
iteration, swapping `.next()` and `.next_back()`, but only if the provided
condition is true. If the condition is false, then iteration proceeds normally.

The condition is only evaluated when the adapter is constructed, and all calls
to drive the iterator are branchless.

## Usage

This can be constructed directly with `Bidi::new(some_iterator)`, but it is more
conveniently accessed as an extension method on double-ended iterators. Import
`wyz::BidiIterator` or `wyz::bidi::*;` and then call `.bidi()` in your iterator
adapter sequence.

## Examples

This can be used to hand-roll a `memmove` implementation that correctly handles
the case where the destination begins in the source region:

```rust
use wyz::bidi::*;

unsafe fn memmove<T>(from: *const T, to: *mut T, count: usize) {
 let src = from .. from.add(count);
 let rev = src.contains(&(to as *const T));
 for idx in (0 .. count).bidi(rev) {
  to.add(idx).write(from.add(idx).read());
 }
}
```

This example detects if `to` is between `from` and `from.add(count)` and uses
that to determine whether to iterate forward from `0` to `count - 1` or backward
from `count - 1` to `0`.
**/
pub struct Bidi<I>
where I: DoubleEndedIterator
{
	/// The iterator being governed.
	inner: I,
	/// A pointer to either `I::next` or `I::next_back`.
	next: fn(&mut I) -> Option<<I as Iterator>::Item>,
	/// A pointer to either `I::next_back` or `I::next`.
	next_back: fn(&mut I) -> Option<<I as Iterator>::Item>,
	/// A pointer to either `I::nth` or `I::nth_back`.
	nth: fn(&mut I, usize) -> Option<<I as Iterator>::Item>,
	/// A pointer to either `I::nth_back` or `I::nth`.
	nth_back: fn(&mut I, usize) -> Option<<I as Iterator>::Item>,
}

impl<I> Bidi<I>
where I: DoubleEndedIterator
{
	/// Applies the `Bidi` adapter to a double-ended iterator and selects the
	/// direction of traversal.
	///
	/// ## Parameters
	///
	/// - `iter`: anything that can be made into a double-ended iterator
	/// - `cond`: determines whether iteration proceeds ordinarily or reversed
	pub fn new<II>(iter: II, cond: bool) -> Self
	where II: IntoIterator<IntoIter = I> {
		let inner = iter.into_iter();
		if cond {
			Self {
				inner,
				next: <I as DoubleEndedIterator>::next_back,
				next_back: <I as Iterator>::next,
				nth: <I as DoubleEndedIterator>::nth_back,
				nth_back: <I as Iterator>::nth,
			}
		}
		else {
			Self {
				inner,
				next: <I as Iterator>::next,
				next_back: <I as DoubleEndedIterator>::next_back,
				nth: <I as Iterator>::nth,
				nth_back: <I as DoubleEndedIterator>::nth_back,
			}
		}
	}
}

impl<I> Iterator for Bidi<I>
where I: DoubleEndedIterator
{
	type Item = <I as Iterator>::Item;

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		(&mut self.next)(&mut self.inner)
	}

	#[inline]
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		(&mut self.nth)(&mut self.inner, n)
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	fn size_hint(&self) -> (usize, Option<usize>) {
		self.inner.size_hint()
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	fn count(self) -> usize {
		self.inner.count()
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

impl<I> DoubleEndedIterator for Bidi<I>
where I: DoubleEndedIterator
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		(&mut self.next_back)(&mut self.inner)
	}

	#[inline]
	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		(&mut self.nth_back)(&mut self.inner, n)
	}
}

impl<I> ExactSizeIterator for Bidi<I>
where I: DoubleEndedIterator + ExactSizeIterator
{
	#[inline]
	#[cfg(not(tarpaulin_include))]
	fn len(&self) -> usize {
		self.inner.len()
	}
}

impl<I> FusedIterator for Bidi<I> where I: DoubleEndedIterator + FusedIterator
{
}

/// Extension trait that provides `.bidi()` for all double-ended iterators.
pub trait BidiIterator
where
	Self: Sized + IntoIterator,
	<Self as IntoIterator>::IntoIter: DoubleEndedIterator,
{
	/// Conditionally reverses the direction of iteration.
	///
	/// When `cond` is true, this adapter swaps the `next` and `nth` methods
	/// with `next_back` and `nth_back`. The resulting iterator is equivalent to
	/// `if cond { self.rev() } else { self }`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use wyz::BidiIterator;
	///
	/// let data = [1, 2, 3];
	/// let mut iter = data.iter().copied().bidi(false);
	/// assert_eq!(iter.next(), Some(1));
	/// assert_eq!(iter.next_back(), Some(3));
	///
	/// let mut iter = data.iter().copied().bidi(true);
	/// assert_eq!(iter.next(), Some(3));
	/// assert_eq!(iter.next_back(), Some(1));
	/// ```
	fn bidi(self, cond: bool) -> Bidi<Self::IntoIter> {
		Bidi::new(self, cond)
	}
}

impl<I> BidiIterator for I
where
	I: Sized + IntoIterator,
	<I as IntoIterator>::IntoIter: DoubleEndedIterator,
{
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn forward() {
		let mut iter = (0 .. 6).bidi(false);

		assert_eq!(iter.next(), Some(0));
		assert_eq!(iter.next_back(), Some(5));
		assert_eq!(iter.nth(1), Some(2));
		assert_eq!(iter.nth_back(1), Some(3));
		assert!(iter.next().is_none());
	}

	#[test]
	fn reverse() {
		let mut iter = (0 .. 6).bidi(true);

		assert_eq!(iter.next(), Some(5));
		assert_eq!(iter.next_back(), Some(0));
		assert_eq!(iter.nth(1), Some(3));
		assert_eq!(iter.nth_back(1), Some(2));
		assert!(iter.next().is_none());
	}
}
