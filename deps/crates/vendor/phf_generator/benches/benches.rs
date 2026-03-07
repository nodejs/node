use criterion::measurement::Measurement;
use criterion::{criterion_group, criterion_main, Bencher, BenchmarkId, Criterion};

use rand::distributions::Standard;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};

use phf_generator::generate_hash;

fn gen_vec(len: usize) -> Vec<u64> {
    SmallRng::seed_from_u64(0xAAAAAAAAAAAAAAAA)
        .sample_iter(Standard)
        .take(len)
        .collect()
}

fn bench_hash<M: Measurement>(b: &mut Bencher<M>, len: &usize) {
    let vec = gen_vec(*len);
    b.iter(|| generate_hash(&vec))
}

fn gen_hash_small(c: &mut Criterion) {
    let sizes = vec![0, 1, 2, 5, 10, 25, 50, 75];
    for size in &sizes {
        c.bench_with_input(BenchmarkId::new("gen_hash_small", *size), size, bench_hash);
    }
}

fn gen_hash_med(c: &mut Criterion) {
    let sizes = vec![100, 250, 500, 1000, 2500, 5000, 7500];
    for size in &sizes {
        c.bench_with_input(BenchmarkId::new("gen_hash_medium", *size), size, bench_hash);
    }
}

fn gen_hash_large(c: &mut Criterion) {
    let sizes = vec![10_000, 25_000, 50_000, 75_000];
    for size in &sizes {
        c.bench_with_input(BenchmarkId::new("gen_hash_large", *size), size, bench_hash);
    }
}

fn gen_hash_xlarge(c: &mut Criterion) {
    let sizes = vec![100_000, 250_000, 500_000, 750_000, 1_000_000];
    for size in &sizes {
        c.bench_with_input(BenchmarkId::new("gen_hash_xlarge", *size), size, bench_hash);
    }
}

criterion_group!(
    benches,
    gen_hash_small,
    gen_hash_med,
    gen_hash_large,
    gen_hash_xlarge
);

criterion_main!(benches);
