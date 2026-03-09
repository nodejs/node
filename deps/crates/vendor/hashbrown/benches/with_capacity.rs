#![feature(test)]

extern crate test;

use hashbrown::HashMap;
use test::{black_box, Bencher};

type Map<K, V> = HashMap<K, V>;

macro_rules! bench_with_capacity {
    ($name:ident, $cap:expr) => {
        #[bench]
        fn $name(b: &mut Bencher) {
            b.iter(|| {
                // Construct a new empty map with a given capacity and return it to avoid
                // being optimized away. Dropping it measures allocation + minimal setup.
                let m: Map<usize, usize> = Map::with_capacity($cap);
                black_box(m)
            });
        }
    };
}

bench_with_capacity!(with_capacity_000000, 0);
bench_with_capacity!(with_capacity_000001, 1);
bench_with_capacity!(with_capacity_000003, 3);
bench_with_capacity!(with_capacity_000007, 7);
bench_with_capacity!(with_capacity_000008, 8);
bench_with_capacity!(with_capacity_000016, 16);
bench_with_capacity!(with_capacity_000032, 32);
bench_with_capacity!(with_capacity_000064, 64);
bench_with_capacity!(with_capacity_000128, 128);
bench_with_capacity!(with_capacity_000256, 256);
bench_with_capacity!(with_capacity_000512, 512);
bench_with_capacity!(with_capacity_001024, 1024);
bench_with_capacity!(with_capacity_004096, 4096);
bench_with_capacity!(with_capacity_016384, 16384);
bench_with_capacity!(with_capacity_065536, 65536);
