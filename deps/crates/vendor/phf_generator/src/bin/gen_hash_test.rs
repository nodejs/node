use criterion::*;

use rand::distributions::Alphanumeric;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};

use phf_generator::generate_hash;

fn gen_vec(len: usize) -> Vec<String> {
    let mut rng = SmallRng::seed_from_u64(0xAAAAAAAAAAAAAAAA).sample_iter(Alphanumeric);

    (0..len)
        .map(move |_| rng.by_ref().take(64).collect::<String>())
        .collect()
}

fn main() {
    let data = black_box(gen_vec(1_000_000));
    black_box(generate_hash(&data));
}
