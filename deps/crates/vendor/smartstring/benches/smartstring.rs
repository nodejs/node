use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion, Throughput};
use rand::{distributions::Standard, Rng, SeedableRng};
use smartstring::{Compact, LazyCompact, SmartString, SmartStringMode};
use std::collections::BTreeSet;

const SIZES: &[usize] = &[4096, 16384, 32768, 65536, 131072];
// const SIZES: &[usize] = &[4096, 65536];

// Makes a random string of only ASCII chars.
fn make_key(chars: &mut impl Iterator<Item = u8>, key_len: usize) -> String {
    let mut key = String::with_capacity(key_len);
    loop {
        let ch: char = ((chars.next().unwrap() % 96) + 32).into();
        key.push(ch);
        if key.len() >= key_len {
            return key;
        }
    }
}

fn make_indices(count: usize, key_len: usize) -> Vec<String> {
    let mut chars = rand::rngs::StdRng::seed_from_u64(31337).sample_iter::<u8, _>(Standard);
    let mut control = BTreeSet::new();
    let mut indices = Vec::new();
    loop {
        let key: String = make_key(&mut chars, key_len);
        if control.contains(&key) {
            continue;
        }
        control.insert(key.clone());
        indices.push(key);
        if indices.len() >= count {
            break;
        }
    }
    indices
}

fn make_set<A: Ord + Clone>(indices: &Vec<A>) -> BTreeSet<A> {
    let mut set = BTreeSet::new();
    for key in indices {
        set.insert(key.clone());
    }
    set
}

fn make_string_input(count: usize, key_len: usize) -> (Vec<String>, BTreeSet<String>) {
    let indices = make_indices(count, key_len);
    let set = make_set(&indices);
    (indices, set)
}

fn make_smart_input<Mode>(
    indices: &Vec<String>,
    set: &BTreeSet<String>,
) -> (Vec<SmartString<Mode>>, BTreeSet<SmartString<Mode>>)
where
    Mode: SmartStringMode,
{
    (
        indices.iter().cloned().map(From::from).collect(),
        set.iter().cloned().map(From::from).collect(),
    )
}

fn lookup_random(key_size: usize, c: &mut Criterion) {
    let mut group = c.benchmark_group(format!("BTreeMap random lookup/key_len={}", key_size));
    for size in SIZES {
        group.throughput(Throughput::Elements(*size as u64));
        let (string_indices, string_set) = make_string_input(*size, key_size);
        let (smartc_indices, smartc_set) =
            make_smart_input::<Compact>(&string_indices, &string_set);
        let (smartp_indices, smartp_set) =
            make_smart_input::<LazyCompact>(&string_indices, &string_set);

        group.bench_function(BenchmarkId::new("String", size), |b| {
            b.iter(|| {
                for k in &string_indices {
                    black_box(string_set.contains(k));
                }
            })
        });

        group.bench_function(BenchmarkId::new("SmartString<Compact>", size), |b| {
            b.iter(|| {
                for k in &smartc_indices {
                    black_box(smartc_set.contains(k));
                }
            })
        });

        group.bench_function(BenchmarkId::new("SmartString<LazyCompact>", size), |b| {
            b.iter(|| {
                for k in &smartp_indices {
                    black_box(smartp_set.contains(k));
                }
            })
        });
    }
    group.finish();
}

fn lookup_random_16b(c: &mut Criterion) {
    lookup_random(16, c)
}

fn lookup_random_256b(c: &mut Criterion) {
    lookup_random(256, c)
}

fn lookup_random_4096b(c: &mut Criterion) {
    lookup_random(4096, c)
}

criterion_group!(
    smartstring,
    lookup_random_16b,
    lookup_random_256b,
    lookup_random_4096b
);
criterion_main!(smartstring);
