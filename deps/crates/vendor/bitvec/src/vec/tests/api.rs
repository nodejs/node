use crate::prelude::*;

#[test]
fn ins_del() {
	let mut bv = bitvec![0, 1, 0, 0, 1];

	assert!(!bv.swap_remove(2));
	assert_eq!(bv, bits![0, 1, 1, 0]);

	bv.insert(2, false);
	assert_eq!(bv, bits![0, 1, 0, 1, 0]);

	assert!(bv.remove(3));
	assert_eq!(bv, bits![0, 1, 0, 0]);
}

#[test]
fn walk() {
	let mut bv = bitvec![
		0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0
	];
	assert_eq!(bv.pop(), Some(false));
	assert_eq!(bv.count_ones(), 8);

	bv.retain(|idx, &bit| bit && idx % 2 == 1);
	assert_eq!(bv, bits![1; 7]);

	let mut bv2 = bitvec![1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1];
	bv.append(&mut bv2);
	assert_eq!(bv.count_ones(), 14);
	assert!(bv2.is_empty());

	let mut splice = bv.splice(2 .. 10, Some(false));
	assert!(splice.all(|bit| bit));
	drop(splice);
	assert_eq!(bv, bits![1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1]);
}

#[test]
fn misc() {
	let mut bv = bitvec![0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1];
	let bv2 = bv.split_off(10);
	assert_eq!(bv2, bits![0, 1, 0, 1]);
	bv.clear();

	let mut a = 1;
	let mut b = 1;
	let fib = |idx| {
		if idx == a.max(b) {
			let c = a + b;
			b = a;
			a = c;
			true
		}
		else {
			false
		}
	};
	bv.resize_with(22, fib);
	assert_eq!(bv, bits![
		0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
	]);

	bv.resize(14, false);
	assert_eq!(bv, bits![0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1]);

	let mut bv = bitvec![0, 0, 1, 1, 0, 0];
	bv.extend_from_within(2 .. 4);
	assert_eq!(bv, bits![0, 0, 1, 1, 0, 0, 1, 1]);
}
