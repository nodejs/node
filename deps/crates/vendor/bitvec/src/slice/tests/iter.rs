#![cfg(test)]

use crate::prelude::*;

#[test]
fn iter() {
	let bits = bits![0, 1, 0, 1, 0, 1];
	let mut iter = bits.iter().by_refs();

	assert!(!*iter.next().unwrap());
	assert!(!*iter.nth(1).unwrap());
	assert!(*iter.next_back().unwrap());
	assert!(*iter.nth_back(1).unwrap());

	assert_eq!(iter.len(), 0);
	assert!(iter.next().is_none());
	assert!(iter.next_back().is_none());
	assert!(iter.nth(1).is_none());
	assert!(iter.nth_back(1).is_none());
}

#[test]
fn iter_mut() {
	let bits = bits![mut 0, 1, 0, 0, 1];
	let mut iter = bits.iter_mut();
	while let Some(mut bit) = iter.nth(1) {
		*bit = !*bit;
	}
	assert_eq!(bits, bits![0, 0, 0, 1, 1]);
}

#[test]
fn windows() {
	let bits = bits![0, 1, 0, 1, 1, 0, 0, 1, 1, 1];
	let base = bits.as_bitptr();
	let mut windows = bits.windows(4);
	assert_eq!(windows.len(), 7);

	let next = windows.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 1, 0, 1]);

	let next_back = windows.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 6);
	assert_eq!(next_back, bits![0, 1, 1, 1]);

	let nth = windows.nth(2).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 3);
	assert_eq!(nth, bits![1, 1, 0, 0]);

	let nth_back = windows.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 4);
	assert_eq!(nth_back, bits![1, 0, 0, 1]);

	assert_eq!(windows.len(), 0);
	assert!(windows.next().is_none());
	assert!(windows.next_back().is_none());
	assert!(windows.nth(1).is_none());
	assert!(windows.nth_back(1).is_none());
}

#[test]
fn chunks() {
	let bits = bits![0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0];
	//               ^^^^^       ^^^^^ ^^^^^       ^
	let base = bits.as_bitptr();
	let mut chunks = bits.chunks(2);
	assert_eq!(chunks.len(), 6);

	let next = chunks.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0]);

	let next_back = chunks.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 10);
	assert_eq!(next_back, bits![0]);

	let nth = chunks.nth(1).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 4);
	assert_eq!(nth, bits![0, 1]);

	let nth_back = chunks.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 6);
	assert_eq!(nth_back, bits![1, 0]);

	assert_eq!(chunks.len(), 0);
	assert!(chunks.next().is_none());
	assert!(chunks.next_back().is_none());
	assert!(chunks.nth(1).is_none());
	assert!(chunks.nth_back(1).is_none());

	assert_eq!(bits![0; 2].chunks(3).next().unwrap().len(), 2);
	assert_eq!(bits![0; 5].chunks(3).next().unwrap().len(), 3);
	assert_eq!(bits![0; 5].chunks(3).nth(1).unwrap().len(), 2);
	assert_eq!(bits![0; 8].chunks(3).nth(1).unwrap().len(), 3);

	assert_eq!(bits![0; 5].chunks(3).next_back().unwrap().len(), 2);
	assert_eq!(bits![0; 6].chunks(3).next_back().unwrap().len(), 3);
	assert_eq!(bits![0; 5].chunks(3).nth_back(1).unwrap().len(), 3);
}

#[test]
fn chunks_mut() {
	let bits = bits![mut 1; 11];
	let base = bits.as_bitptr();
	let mut chunks = bits.chunks_mut(2);
	assert_eq!(chunks.len(), 6);

	let next = chunks.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	next.fill(false);

	let next_back = chunks.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 10);
	next_back.fill(false);

	let nth = chunks.nth(1).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 4);
	nth.set(0, false);

	let nth_back = chunks.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 6);
	nth_back.set(1, false);

	assert_eq!(chunks.len(), 0);
	assert!(chunks.next().is_none());
	assert!(chunks.next_back().is_none());
	assert!(chunks.nth(1).is_none());
	assert!(chunks.nth_back(1).is_none());

	assert_eq!(bits, bits![0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0]);

	assert_eq!(bits![mut 0; 2].chunks_mut(3).next().unwrap().len(), 2);
	assert_eq!(bits![mut 0; 5].chunks_mut(3).next().unwrap().len(), 3);
	assert_eq!(bits![mut 0; 5].chunks_mut(3).nth(1).unwrap().len(), 2);
	assert_eq!(bits![mut 0; 8].chunks_mut(3).nth(1).unwrap().len(), 3);

	assert_eq!(bits![mut 0; 5].chunks_mut(3).next_back().unwrap().len(), 2);
	assert_eq!(bits![mut 0; 6].chunks_mut(3).next_back().unwrap().len(), 3);
	assert_eq!(bits![mut 0; 5].chunks_mut(3).nth_back(1).unwrap().len(), 3);
}

#[test]
fn chunks_exact() {
	let bits = bits![
		0, 0, 0, 1, 1, 1, 0, 0, 1, // next and nth(1)
		1, 0, 0, 1, 1, 1, 0, 1, 0, // next_back and nth_back(1)
		1, 1, // remainder
	];
	let base = bits.as_bitptr();
	let mut chunks = bits.chunks_exact(3);
	assert_eq!(chunks.len(), 6);

	let next = chunks.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0, 0]);

	let nth = chunks.nth(1).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 6);
	assert_eq!(nth, bits![0, 0, 1]);

	let next_back = chunks.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 15);
	assert_eq!(next_back, bits![0, 1, 0]);

	let nth_back = chunks.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 9);
	assert_eq!(nth_back, bits![1, 0, 0]);

	let remainder = chunks.remainder();
	assert_eq!(unsafe { remainder.as_bitptr().offset_from(base) }, 18);
	assert_eq!(remainder, bits![1, 1]);

	assert_eq!(chunks.len(), 0);
	assert!(chunks.next().is_none());
	assert!(chunks.next_back().is_none());
	assert!(chunks.nth(1).is_none());
	assert!(chunks.nth_back(1).is_none());
}

#[test]
fn chunks_exact_mut() {
	let bits = bits![mut 0; 20];
	let base = bits.as_bitptr();
	let mut chunks = bits.chunks_exact_mut(3);

	let next = chunks.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	next.fill(true);

	let next_back = chunks.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 15);
	next_back.fill(true);

	let nth = chunks.nth(1).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 6);
	nth.set(2, true);

	let nth_back = chunks.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 9);
	nth_back.set(0, true);

	assert_eq!(chunks.len(), 0);
	assert!(chunks.next().is_none());
	assert!(chunks.next_back().is_none());
	assert!(chunks.nth(1).is_none());
	assert!(chunks.nth_back(1).is_none());

	assert_eq!(bits, bits![
		1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
	]);
	bits.fill(false);

	let mut chunks = bits.chunks_exact_mut(3);
	let remainder = chunks.take_remainder();
	assert_eq!(unsafe { remainder.as_bitptr().offset_from(base) }, 18);
	remainder.fill(true);
	assert!(chunks.take_remainder().is_empty());
	assert!(chunks.into_remainder().is_empty());
	assert!(bits.ends_with(bits![0, 0, 1, 1]));
}

#[test]
fn rchunks() {
	let bits = bits![1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0];
	//               ^^       ^^^^^ ^^^^^       ^^^^
	let base = bits.as_bitptr();
	let mut rchunks = bits.rchunks(2);

	let next = rchunks.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 9);
	assert_eq!(next, bits![0, 0]);

	let next_back = rchunks.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next_back, bits![1]);

	let nth = rchunks.nth(1).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 5);
	assert_eq!(nth, bits![0, 1]);

	let nth_back = rchunks.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 3);
	assert_eq!(nth_back, bits![1, 0]);

	assert_eq!(rchunks.len(), 0);
	assert!(rchunks.next().is_none());
	assert!(rchunks.next_back().is_none());
	assert!(rchunks.nth(1).is_none());
	assert!(rchunks.nth_back(1).is_none());

	assert_eq!(bits![0; 5].rchunks(3).next().unwrap().len(), 3);
	assert_eq!(bits![0; 5].rchunks(3).nth(1).unwrap().len(), 2);
	assert_eq!(bits![0; 5].rchunks(3).next_back().unwrap().len(), 2);
	assert_eq!(bits![0; 5].rchunks(3).nth_back(1).unwrap().len(), 3);
}

#[test]
fn rchunks_mut() {
	let bits = bits![mut 0; 11];
	let base = bits.as_bitptr();
	let mut rchunks = bits.rchunks_mut(2);
	assert_eq!(rchunks.len(), 6);

	let next = rchunks.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 9);
	next.fill(true);

	let next_back = rchunks.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 0);
	next_back.fill(true);

	let nth = rchunks.nth(1).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 5);
	nth.set(0, true);

	let nth_back = rchunks.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 3);
	nth_back.set(1, true);

	assert_eq!(rchunks.len(), 0);
	assert!(rchunks.next().is_none());
	assert!(rchunks.next_back().is_none());
	assert!(rchunks.nth(1).is_none());
	assert!(rchunks.nth_back(1).is_none());

	assert_eq!(bits, bits![1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1]);

	assert_eq!(bits![mut 0; 5].rchunks_mut(3).next().unwrap().len(), 3);
	assert_eq!(bits![mut 0; 5].rchunks_mut(3).nth(1).unwrap().len(), 2);
	assert_eq!(bits![mut 0; 5].rchunks_mut(3).next_back().unwrap().len(), 2);
	assert_eq!(bits![mut 0; 5].rchunks_mut(3).nth_back(1).unwrap().len(), 3);
}

#[test]
fn rchunks_exact() {
	let bits = bits![
		1, 1, // remainder
		0, 1, 0, 1, 1, 1, 0, 0, 1, // nth_back(1) and next
		1, 0, 0, 1, 1, 1, 0, 0, 0, // nth(1) and next
	];
	let base = bits.as_bitptr();
	let mut rchunks = bits.rchunks_exact(3);
	assert_eq!(rchunks.len(), 6);

	let next = rchunks.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 17);
	assert_eq!(next, bits![0, 0, 0]);

	let nth = rchunks.nth(1).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 11);
	assert_eq!(nth, bits![1, 0, 0]);

	let next_back = rchunks.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 2);
	assert_eq!(next_back, bits![0, 1, 0]);

	let nth_back = rchunks.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 8);
	assert_eq!(nth_back, bits![0, 0, 1]);

	let remainder = rchunks.remainder();
	assert_eq!(unsafe { remainder.as_bitptr().offset_from(base) }, 0);
	assert_eq!(remainder, bits![1, 1]);

	assert_eq!(rchunks.len(), 0);
	assert!(rchunks.next().is_none());
	assert!(rchunks.next_back().is_none());
	assert!(rchunks.nth(1).is_none());
	assert!(rchunks.nth_back(1).is_none());
}

#[test]
fn rchunks_exact_mut() {
	let bits = bits![mut 0; 20];
	let base = bits.as_bitptr();
	let mut rchunks = bits.rchunks_exact_mut(3);

	let next = rchunks.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 17);
	next.fill(true);

	let next_back = rchunks.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 2);
	next_back.fill(true);

	let nth = rchunks.nth(1).unwrap();
	assert_eq!(unsafe { nth.as_bitptr().offset_from(base) }, 11);
	nth.set(2, true);

	let nth_back = rchunks.nth_back(1).unwrap();
	assert_eq!(unsafe { nth_back.as_bitptr().offset_from(base) }, 8);
	nth_back.set(0, true);

	assert_eq!(rchunks.len(), 0);
	assert!(rchunks.next().is_none());
	assert!(rchunks.next_back().is_none());
	assert!(rchunks.nth(1).is_none());
	assert!(rchunks.nth_back(1).is_none());

	assert_eq!(bits, bits![
		0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1
	]);
	bits.fill(false);

	let mut chunks = bits.rchunks_exact_mut(3);
	let remainder = chunks.take_remainder();
	assert_eq!(unsafe { remainder.as_bitptr().offset_from(base) }, 0);
	remainder.fill(true);
	assert!(chunks.take_remainder().is_empty());
	assert!(chunks.into_remainder().is_empty());
	assert!(bits.starts_with(bits![1, 1, 0, 0]));
}

#[test]
fn split() {
	let bits = bits![0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut split = bits.split(|_, &bit| bit);

	let next = split.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0]);

	let next = split.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 3);
	assert!(next.is_empty());

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 8);
	assert!(next_back.is_empty());

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 7);
	assert!(next_back.is_empty());

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 4);
	assert_eq!(next_back, bits![0, 0]);

	assert!(split.next().is_none());
	assert!(split.next_back().is_none());
}

#[test]
fn split_mut() {
	let bits = bits![mut 0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut split = bits.split_mut(|_, &bit| bit);

	let next = split.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0]);

	let next = split.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 3);
	assert!(next.is_empty());

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 8);
	assert!(next_back.is_empty());

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 7);
	assert!(next_back.is_empty());

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 4);
	assert_eq!(next_back, bits![0, 0]);

	assert!(split.next().is_none());
	assert!(split.next_back().is_none());

	let bits = bits![mut 0];
	let mut split = bits.split_mut(|_, &bit| bit);
	assert_eq!(split.next().unwrap(), bits![0]);
	assert!(split.next().is_none());
}

#[test]
fn split_inclusive() {
	let bits = bits![0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut split = bits.split_inclusive(|_, &bit| bit);

	let next = split.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0, 1]);

	let next = split.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 3);
	assert_eq!(next, bits![1]);

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 7);
	assert_eq!(next_back, bits![1]);

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 4);
	assert_eq!(next_back, bits![0, 0, 1]);

	assert!(split.next().is_none());
	assert!(split.next_back().is_none());

	let bits = bits![0, 1];
	let mut split = bits.split_inclusive(|_, &bit| bit);
	assert_eq!(split.next(), Some(bits![0, 1]));
	assert!(split.next().is_none());
	let mut split = bits.split_inclusive(|_, &bit| bit);
	assert_eq!(split.next_back(), Some(bits![0, 1]));
	assert!(split.next_back().is_none());

	assert_eq!(
		bits![].split_inclusive(|_, &bit| bit).next_back(),
		Some(bits![]),
	);
}

#[test]
fn split_inclusive_mut() {
	let bits = bits![mut 0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut split = bits.split_inclusive_mut(|_, &bit| bit);

	let next = split.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0, 1]);

	let next = split.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 3);
	assert_eq!(next, bits![1]);

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 7);
	assert_eq!(next_back, bits![1]);

	let next_back = split.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 4);
	assert_eq!(next_back, bits![0, 0, 1]);

	assert!(split.next().is_none());
	assert!(split.next_back().is_none());

	let bits = bits![mut 0, 1];
	let mut split = bits.split_inclusive_mut(|_, &bit| bit);
	assert_eq!(split.next().unwrap(), bits![0, 1]);
	assert!(split.next().is_none());
	let mut split = bits.split_inclusive_mut(|_, &bit| bit);
	assert_eq!(split.next_back().unwrap(), bits![0, 1]);
	assert!(split.next_back().is_none());

	assert_eq!(
		bits![mut]
			.split_inclusive_mut(|_, &bit| bit)
			.next_back()
			.unwrap(),
		bits![],
	);
}

#[test]
fn rsplit() {
	let bits = bits![0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut rsplit = bits.rsplit(|_, &bit| bit);

	let next = rsplit.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 8);
	assert!(next.is_empty());

	let next = rsplit.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 7);
	assert!(next.is_empty());

	let next_back = rsplit.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next_back, bits![0, 0]);

	let next_back = rsplit.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 3);
	assert!(next_back.is_empty());

	let next_back = rsplit.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 4);
	assert_eq!(next_back, bits![0, 0]);

	assert!(rsplit.next().is_none());
	assert!(rsplit.next_back().is_none());
}

#[test]
fn rsplit_mut() {
	let bits = bits![mut 0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut rsplit = bits.rsplit_mut(|_, &bit| bit);

	let next = rsplit.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 8);
	assert!(next.is_empty());

	let next = rsplit.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 7);
	assert!(next.is_empty());

	let next_back = rsplit.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next_back, bits![0, 0]);

	let next_back = rsplit.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 3);
	assert!(next_back.is_empty());

	let next_back = rsplit.next_back().unwrap();
	assert_eq!(unsafe { next_back.as_bitptr().offset_from(base) }, 4);
	assert!(next.is_empty());

	assert!(rsplit.next().is_none());
	assert!(rsplit.next_back().is_none());

	let bits = bits![mut 0];
	let mut rsplit = bits.rsplit_mut(|_, &bit| bit);
	assert_eq!(rsplit.next().unwrap(), bits![0]);
	assert!(rsplit.next().is_none());
}

#[test]
fn splitn() {
	let bits = bits![0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut splitn = bits.splitn(2, |_, &bit| bit);

	let next = splitn.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0]);

	let next = splitn.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 3);
	assert_eq!(next, bits[3 ..]);

	assert!(splitn.next().is_none());
}

#[test]
fn splitn_mut() {
	let bits = bits![mut 0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut splitn = bits.splitn_mut(2, |_, &bit| bit);

	let next = splitn.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0]);

	let next = splitn.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 3);
	assert_eq!(next, bits![1, 0, 0, 1, 1]);

	assert!(splitn.next().is_none());
}

#[test]
fn rsplitn() {
	let bits = bits![0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut rsplitn = bits.rsplitn(2, |_, &bit| bit);

	let next = rsplitn.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 8);
	assert!(next.is_empty());

	let next = rsplitn.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0, 1, 1, 0, 0, 1]);
}

#[test]
fn rsplitn_mut() {
	let bits = bits![mut 0, 0, 1, 1, 0, 0, 1, 1];
	let base = bits.as_bitptr();
	let mut rsplitn = bits.rsplitn_mut(2, |_, &bit| bit);

	let next = rsplitn.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 8);
	assert!(next.is_empty());

	let next = rsplitn.next().unwrap();
	assert_eq!(unsafe { next.as_bitptr().offset_from(base) }, 0);
	assert_eq!(next, bits![0, 0, 1, 1, 0, 0, 1]);

	assert!(rsplitn.next().is_none());
}

#[test]
fn iter_ones() {
	use crate::order::HiLo;

	let bits = 0b0100_1001u8.view_bits::<HiLo>();
	// ordering: 3210 7654
	let mut ones = bits.iter_ones();
	assert_eq!(ones.len(), 3);
	assert_eq!(ones.next(), Some(2));
	assert_eq!(ones.next_back(), Some(7));
	assert_eq!(ones.next(), Some(4));
	assert!(ones.next().is_none());
}

#[test]
fn iter_zeros() {
	use crate::order::HiLo;

	let bits = 0b1011_0110u8.view_bits::<HiLo>();
	// ordering: 3210 7654
	let mut zeros = bits.iter_zeros();
	assert_eq!(zeros.len(), 3);
	assert_eq!(zeros.next(), Some(2));
	assert_eq!(zeros.next_back(), Some(7));
	assert_eq!(zeros.next(), Some(4));
	assert!(zeros.next().is_none());
}

#[test]
fn trait_impls() {
	use core::iter::FusedIterator;

	use static_assertions::*;

	use crate::slice::iter::{
		BitRefIter,
		BitValIter,
	};

	assert_impl_all!(
		BitRefIter<'static, usize, Lsb0>: Iterator,
		DoubleEndedIterator,
		ExactSizeIterator,
		FusedIterator
	);
	assert_impl_all!(
		BitValIter<'static, usize, Lsb0>: Iterator,
		DoubleEndedIterator,
		ExactSizeIterator,
		FusedIterator
	);
}
