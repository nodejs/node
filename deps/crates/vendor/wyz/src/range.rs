//! Range utilities.

use core::ops::{
	Bound,
	Range,
	RangeBounds,
};

/// Extension methods for working with various range types.
pub trait RangeExt<T>: RangeBounds<T>
where T: Ord
{
	/// Normalizes a range-like type to a canonical half-open `Range`.
	///
	/// ## Parameters
	///
	/// - `self`: The range to normalize.
	/// - `start`: An optional fallback *inclusive* lower bound.
	/// - `end`: An optional fallback *exclusive* upper bound.
	///
	/// ## Returns
	///
	/// A `Range` whose start and end values are the following, in order of
	/// decreasing priority:
	///
	/// - `self.start()`, or if absent, the `start` parameter, or if it is
	///   `None`, `0`.
	/// - `self.end()`, or if absent, the `end` parameter, or if it is `None`,
	///   !0`.
	fn normalize(
		self,
		start: impl Into<Option<T>>,
		end: impl Into<Option<T>>,
	) -> Range<T>;

	/// Finds the intersection between two range-likes. The produced `Range`
	/// spans only the elements common to both.
	///
	/// This returns `None` if the ranges do not have an intersection (at least
	/// one element present in both ranges).
	fn intersection<R>(self, other: R) -> Option<Range<T>>
	where R: RangeExt<T>;

	/// Finds the union between two range-likes. The produced `Range` spans all
	/// elements present in at least one of them.
	///
	/// This returns `None` if the ranges do not have an intersection (at least
	/// one element present in both ranges).
	fn union<R>(self, other: R) -> Option<Range<T>>
	where R: RangeExt<T>;
}

//  TODO(myrrlyn): Use funty to extend this for all integers.
impl<R> RangeExt<usize> for R
where R: RangeBounds<usize>
{
	fn normalize(
		self,
		start: impl Into<Option<usize>>,
		end: impl Into<Option<usize>>,
	) -> Range<usize> {
		let start = match self.start_bound() {
			Bound::Unbounded => start.into().unwrap_or(0),
			Bound::Included(&v) => v,
			Bound::Excluded(&v) => v.saturating_add(1),
		};
		let end = match self.end_bound() {
			Bound::Unbounded => end.into().unwrap_or(!0),
			Bound::Included(&v) => v.saturating_add(1),
			Bound::Excluded(&v) => v,
		};
		if start > end {
			end .. start
		}
		else {
			start .. end
		}
	}

	fn intersection<R2>(self, other: R2) -> Option<Range<usize>>
	where R2: RangeExt<usize> {
		let Range { start: a1, end: a2 } = self.normalize(None, None);
		let Range { start: b1, end: b2 } = other.normalize(None, None);
		if b1 < a1 {
			return (b1 .. b2).intersection(a1 .. a2);
		}
		if !(a1 .. a2).contains(&b1) {
			return None;
		}
		let start = a1.max(b1);
		let end = a2.min(b2);
		if start > end {
			Some(end .. start)
		}
		else {
			Some(start .. end)
		}
	}

	fn union<R2>(self, other: R2) -> Option<Range<usize>>
	where R2: RangeExt<usize> {
		let Range { start: a1, end: a2 } = self.normalize(None, None);
		let Range { start: b1, end: b2 } = other.normalize(None, None);
		if b1 < a1 {
			return (b1 .. b2).intersection(a1 .. a2);
		}
		if !(a1 .. a2).contains(&b1) {
			return None;
		}
		let start = a1.min(b1);
		let end = a2.max(b2);
		if start > end {
			Some(end .. start)
		}
		else {
			Some(start .. end)
		}
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn normalize() {
		let r = (..).normalize(1, 10);
		assert!(r.contains(&1));
		assert!(r.contains(&9));
		assert!(!r.contains(&0));
		assert!(!r.contains(&10));

		let r = (.. 10).normalize(1, 20);
		assert!(r.contains(&1));
		assert!(r.contains(&9));
		assert!(!r.contains(&0));
		assert!(!r.contains(&10));

		let r = (4 ..).normalize(6, 10);
		assert!(r.contains(&4));
		assert!(r.contains(&9));
		assert!(!r.contains(&3));
		assert!(!r.contains(&10));

		let r = (4 ..= 10).normalize(6, 8);
		assert!(r.contains(&4));
		assert!(r.contains(&10));
		assert!(!r.contains(&3));
		assert!(!r.contains(&11));

		let r = (..= 10).normalize(1, 8);
		assert!(r.contains(&1));
		assert!(r.contains(&10));
		assert!(!r.contains(&0));
		assert!(!r.contains(&11));
	}

	#[test]
	fn intersect() {
		let a = 3 .. 10;
		let b = 7 ..= 20;
		assert_eq!(a.intersection(b), Some(7 .. 10));

		let c = 3 .. 10;
		let d = 13 ..= 20;
		assert!(c.intersection(d).is_none());
	}

	#[test]
	fn union() {
		let a = 3 .. 10;
		let b = 7 ..= 20;
		assert_eq!(a.union(b), Some(3 .. 21));

		let c = 3 .. 10;
		let d = 13 ..= 20;
		assert!(c.union(d).is_none());
	}
}
