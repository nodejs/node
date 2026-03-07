//! See [the `phf` crate's documentation][phf] for details.
//!
//! [phf]: https://docs.rs/phf

#![doc(html_root_url = "https://docs.rs/phf_generator/0.11")]
use phf_shared::{HashKey, Hashes, PhfHash};
use rand::distributions::Standard;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};

const DEFAULT_LAMBDA: usize = 5;

const FIXED_SEED: u64 = 1234567890;

pub struct HashState {
    pub key: HashKey,
    pub disps: Vec<(u32, u32)>,
    pub map: Vec<usize>,
}

pub fn generate_hash<H: PhfHash>(entries: &[H]) -> HashState {
    generate_hash_with_hash_fn(entries, phf_shared::hash)
}

pub fn generate_hash_with_hash_fn<T, F>(entries: &[T], hash_fn: F) -> HashState
where
    F: Fn(&T, &HashKey) -> Hashes,
{
    let mut generator = Generator::new(entries.len());

    SmallRng::seed_from_u64(FIXED_SEED)
        .sample_iter(Standard)
        .find(|key| {
            let hashes = entries.iter().map(|entry| hash_fn(entry, key));
            generator.reset(hashes);

            generator.try_generate_hash()
        })
        .map(|key| HashState {
            key,
            disps: generator.disps,
            map: generator.map.into_iter().map(|i| i.unwrap()).collect(),
        })
        .expect("failed to solve PHF")
}

struct Bucket {
    idx: usize,
    keys: Vec<usize>,
}

struct Generator {
    hashes: Vec<Hashes>,
    buckets: Vec<Bucket>,
    disps: Vec<(u32, u32)>,
    map: Vec<Option<usize>>,
    try_map: Vec<u64>,
}

impl Generator {
    fn new(table_len: usize) -> Self {
        let hashes = Vec::with_capacity(table_len);

        let buckets_len = (table_len + DEFAULT_LAMBDA - 1) / DEFAULT_LAMBDA;
        let buckets: Vec<_> = (0..buckets_len)
            .map(|i| Bucket {
                idx: i,
                keys: vec![],
            })
            .collect();
        let disps = vec![(0u32, 0u32); buckets_len];

        let map = vec![None; table_len];
        let try_map = vec![0u64; table_len];

        Self {
            hashes,
            buckets,
            disps,
            map,
            try_map,
        }
    }

    fn reset<I>(&mut self, hashes: I)
    where
        I: Iterator<Item = Hashes>,
    {
        self.buckets.iter_mut().for_each(|b| b.keys.clear());
        self.buckets.sort_by_key(|b| b.idx);
        self.disps.iter_mut().for_each(|d| *d = (0, 0));
        self.map.iter_mut().for_each(|m| *m = None);
        self.try_map.iter_mut().for_each(|m| *m = 0);

        self.hashes.clear();
        self.hashes.extend(hashes);
    }

    fn try_generate_hash(&mut self) -> bool {
        let buckets_len = self.buckets.len() as u32;
        for (i, hash) in self.hashes.iter().enumerate() {
            self.buckets[(hash.g % buckets_len) as usize].keys.push(i);
        }

        // Sort descending
        self.buckets
            .sort_by(|a, b| a.keys.len().cmp(&b.keys.len()).reverse());

        let table_len = self.hashes.len();

        // store whether an element from the bucket being placed is
        // located at a certain position, to allow for efficient overlap
        // checks. It works by storing the generation in each cell and
        // each new placement-attempt is a new generation, so you can tell
        // if this is legitimately full by checking that the generations
        // are equal. (A u64 is far too large to overflow in a reasonable
        // time for current hardware.)
        let mut generation = 0u64;

        // the actual values corresponding to the markers above, as
        // (index, key) pairs, for adding to the main map once we've
        // chosen the right disps.
        let mut values_to_add = vec![];

        'buckets: for bucket in &self.buckets {
            for d1 in 0..(table_len as u32) {
                'disps: for d2 in 0..(table_len as u32) {
                    values_to_add.clear();
                    generation += 1;

                    for &key in &bucket.keys {
                        let idx =
                            (phf_shared::displace(self.hashes[key].f1, self.hashes[key].f2, d1, d2)
                                % (table_len as u32)) as usize;
                        if self.map[idx].is_some() || self.try_map[idx] == generation {
                            continue 'disps;
                        }
                        self.try_map[idx] = generation;
                        values_to_add.push((idx, key));
                    }

                    // We've picked a good set of disps
                    self.disps[bucket.idx] = (d1, d2);
                    for &(idx, key) in &values_to_add {
                        self.map[idx] = Some(key);
                    }
                    continue 'buckets;
                }
            }

            // Unable to find displacements for a bucket
            return false;
        }
        true
    }
}
