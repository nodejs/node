/*! Benchmarks for `BitSlice::copy_from_slice`.

The `copy_from_slice` implementation attempts to detect slice conditions that
allow element-wise `memcpy` behavior, rather than the conservative bit-by-bit
iteration, as element load/stores are faster than reading and writing each bit
in an element individually.
!*/

#![feature(maybe_uninit_uninit_array, maybe_uninit_slice)]

use std::mem::MaybeUninit;

use bitvec::{
	mem::{
		bits_of,
		elts,
	},
	prelude::*,
};
use criterion::{
	criterion_group,
	criterion_main,
	BenchmarkId,
	Criterion,
	SamplingMode,
	Throughput,
};
use tap::Tap;

//  One kibibyte
const KIBIBYTE: usize = 1024;
//  Some number of kibibytes
#[allow(clippy::identity_op)]
const FACTOR: usize = 1 * KIBIBYTE;

//  Scalars applied to FACTOR to get a range of action
const SCALARS: &[usize] = &[1, 2, 4, 8, 16, 24, 32, 40, 52, 64];
//  The maximum number of bits in a memory region.
const MAX_BITS: usize = 64 * FACTOR * 8;

fn make_slots<T, const LEN: usize>()
-> ([MaybeUninit<T>; LEN], [MaybeUninit<T>; LEN])
where T: BitStore {
	(
		MaybeUninit::<T>::uninit_array::<LEN>(),
		MaybeUninit::<T>::uninit_array::<LEN>(),
	)
}

fn view_slots<'a, 'b, T>(
	src: &'a [MaybeUninit<T>],
	dst: &'b mut [MaybeUninit<T>],
) -> (&'a [T], &'b mut [T])
where
	T: BitStore,
{
	unsafe {
		(
			MaybeUninit::slice_assume_init_ref(src),
			MaybeUninit::slice_assume_init_mut(dst),
		)
	}
}

pub fn benchmarks(crit: &mut Criterion) {
	fn steps()
	-> impl Iterator<Item = (impl Fn(&'static str) -> BenchmarkId, usize, Throughput)>
	{
		SCALARS.iter().map(|&n| {
			(
				move |name| BenchmarkId::new(name, n),
				n * FACTOR * bits_of::<u8>(),
				Throughput::Bytes((n * FACTOR) as u64),
			)
		})
	}

	let (src_words, mut dst_words) =
		make_slots::<usize, { elts::<usize>(MAX_BITS) }>();
	let (src_bytes, mut dst_bytes) =
		make_slots::<u8, { elts::<u8>(MAX_BITS) }>();

	let (src_words, dst_words) = view_slots(&src_words, &mut dst_words);
	let (src_bytes, dst_bytes) = view_slots(&src_bytes, &mut dst_bytes);

	macro_rules! mkgrp {
		($crit:ident, $name:literal) => {
			(&mut *$crit).benchmark_group($name).tap_mut(|grp| {
				grp.sampling_mode(SamplingMode::Flat).sample_size(2000);
			})
		};
	}

	let mut group = mkgrp!(crit, "Element-wise");
	for (id, bits, thrpt) in steps() {
		group.throughput(thrpt);
		let words = bits / bits_of::<usize>();
		let bytes = bits / bits_of::<u8>();

		let (src_words, dst_words) =
			(&src_words[.. words], &mut dst_words[.. words]);
		let (src_bytes, dst_bytes) =
			(&src_bytes[.. bytes], &mut dst_bytes[.. bytes]);

		//  Use the builtin memcpy to run the slices in bulk. This ought to be a
		//  lower bound on execution time.

		group.bench_function(id("words_plain"), |b| {
			let (src, dst) = (src_words, &mut *dst_words);
			b.iter(|| dst.copy_from_slice(src))
		});
		group.bench_function(id("bytes_plain"), |b| {
			let (src, dst) = (src_bytes, &mut *dst_bytes);
			b.iter(|| dst.copy_from_slice(src))
		});

		group.bench_function(id("words_manual"), |b| {
			let (src, dst) = (src_words, &mut *dst_words);
			b.iter(|| {
				for (from, to) in src.iter().zip(dst.iter_mut()) {
					*to = *from;
				}
			})
		});
		group.bench_function(id("bytes_manual"), |b| {
			let (src, dst) = (src_bytes, &mut *dst_bytes);
			b.iter(|| {
				for (from, to) in src.iter().zip(dst.iter_mut()) {
					*to = *from;
				}
			})
		});
	}
	group.finish();

	let mut group = mkgrp!(crit, "Bit-wise accelerated");
	for (id, bits, thrpt) in steps() {
		group.throughput(thrpt);
		let words = bits / bits_of::<usize>();
		let bytes = bits / bits_of::<u8>();

		let (src_words, dst_words) =
			(&src_words[.. words], &mut dst_words[.. words]);
		let (src_bytes, dst_bytes) =
			(&src_bytes[.. bytes], &mut dst_bytes[.. bytes]);

		//  Ideal bitwise memcpy: no edges, same typarams, fully aligned.

		group.bench_function(id("bits_words_plain"), |b| {
			let (src, dst) = (
				src_words.view_bits::<Lsb0>(),
				dst_words.view_bits_mut::<Lsb0>(),
			);
			b.iter(|| dst.copy_from_bitslice(src));
		});
		group.bench_function(id("bits_bytes_plain"), |b| {
			let (src, dst) = (
				src_bytes.view_bits::<Lsb0>(),
				dst_bytes.view_bits_mut::<Lsb0>(),
			);
			b.iter(|| dst.copy_from_bitslice(src));
		});

		//  Same typarams, fully aligned, with fuzzed edges.

		group.bench_function(id("bits_words_edges"), |b| {
			let src = src_words.view_bits::<Lsb0>();
			let len = src.len();
			let (src, dst) = (
				&src[10 .. len - 10],
				&mut dst_words.view_bits_mut::<Lsb0>()[10 .. len - 10],
			);
			b.iter(|| dst.copy_from_bitslice(src));
		});
		group.bench_function(id("bits_bytes_edges"), |b| {
			let src = src_bytes.view_bits::<Lsb0>();
			let len = src.len();
			let (src, dst) = (
				&src[10 .. len - 10],
				&mut dst_bytes.view_bits_mut::<Lsb0>()[10 .. len - 10],
			);
			b.iter(|| dst.copy_from_bitslice(src));
		});

		//  Same typarams, misaligned.

		group.bench_function(id("bits_words_misalign"), |b| {
			let src = &src_words.view_bits::<Lsb0>()[10 ..];
			let dst = &mut dst_words.view_bits_mut::<Lsb0>()[.. src.len()];
			b.iter(|| dst.copy_from_bitslice(src));
		});
		group.bench_function(id("bits_bytes_misalign"), |b| {
			let src = &src_bytes.view_bits::<Lsb0>()[10 ..];
			let dst = &mut dst_bytes.view_bits_mut::<Lsb0>()[.. src.len()];
			b.iter(|| dst.copy_from_bitslice(src));
		});
	}
	group.finish();

	let mut group = mkgrp!(crit, "Bit-wise crawl");
	for (id, bits, thrpt) in steps() {
		group.throughput(thrpt);
		let words = bits / bits_of::<usize>();
		let bytes = bits / bits_of::<u8>();

		let (src_words, dst_words) =
			(&src_words[.. words], &mut dst_words[.. words]);
		let (src_bytes, dst_bytes) =
			(&src_bytes[.. bytes], &mut dst_bytes[.. bytes]);

		//  Mismatched type parameters

		group.bench_function(id("bits_words_mismatched"), |b| {
			let (src, dst) = (
				src_words.view_bits::<Msb0>(),
				dst_words.view_bits_mut::<Lsb0>(),
			);
			b.iter(|| dst.clone_from_bitslice(src));
		});
		group.bench_function(id("bits_bytes_mismatched"), |b| {
			let (src, dst) = (
				src_bytes.view_bits::<Msb0>(),
				dst_bytes.view_bits_mut::<Lsb0>(),
			);
			b.iter(|| dst.clone_from_bitslice(src));
		});

		//  Crawl each bit individually. This ought to be an upper bound on
		//  execution time.

		group.bench_function(id("bitwise_words"), |b| {
			let (src, dst) = (
				src_words.view_bits::<Lsb0>(),
				dst_words.view_bits_mut::<Lsb0>(),
			);
			b.iter(|| unsafe {
				for (from, to) in
					src.as_bitptr_range().zip(dst.as_mut_bitptr_range())
				{
					to.write(from.read());
				}
			})
		});
		group.bench_function(id("bitwise_bytes"), |b| {
			let (src, dst) = (
				src_bytes.view_bits::<Lsb0>(),
				dst_bytes.view_bits_mut::<Lsb0>(),
			);
			b.iter(|| unsafe {
				for (from, to) in
					src.as_bitptr_range().zip(dst.as_mut_bitptr_range())
				{
					to.write(from.read());
				}
			})
		});

		group.bench_function(id("bitwise_words_mismatch"), |b| {
			let (src, dst) = (
				src_words.view_bits::<Lsb0>(),
				dst_words.view_bits_mut::<Msb0>(),
			);
			b.iter(|| unsafe {
				for (from, to) in
					src.as_bitptr_range().zip(dst.as_mut_bitptr_range())
				{
					to.write(from.read());
				}
			})
		});
		group.bench_function(id("bitwise_bytes_mismatch"), |b| {
			let (src, dst) = (
				src_bytes.view_bits::<Msb0>(),
				dst_bytes.view_bits_mut::<Lsb0>(),
			);
			b.iter(|| unsafe {
				for (from, to) in
					src.as_bitptr_range().zip(dst.as_mut_bitptr_range())
				{
					to.write(from.read());
				}
			})
		});
	}
}

criterion_group!(benches, benchmarks);
criterion_main!(benches);
