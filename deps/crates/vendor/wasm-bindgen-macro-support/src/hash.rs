//! Common utility function for manipulating syn types and
//! handling parsed values

use std::collections::hash_map::DefaultHasher;
use std::env;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::sync::atomic::AtomicBool;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering::SeqCst;

/// Small utility used when generating symbol names.
///
/// Hashes the public field here along with a few cargo-set env vars to
/// distinguish between runs of the procedural macro.
#[derive(Debug)]
pub struct ShortHash<T>(pub T);

impl<T: Hash> fmt::Display for ShortHash<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        static HASHED: AtomicBool = AtomicBool::new(false);
        static HASH: AtomicUsize = AtomicUsize::new(0);

        // Try to amortize the cost of loading env vars a lot as we're gonna be
        // hashing for a lot of symbols.
        if !HASHED.load(SeqCst) {
            let mut h = DefaultHasher::new();
            env::var("CARGO_PKG_NAME")
                .expect("should have CARGO_PKG_NAME env var")
                .hash(&mut h);
            env::var("CARGO_PKG_VERSION")
                .expect("should have CARGO_PKG_VERSION env var")
                .hash(&mut h);
            // This may chop off 32 bits on 32-bit platforms, but that's ok, we
            // just want something to mix in below anyway.
            HASH.store(h.finish() as usize, SeqCst);
            HASHED.store(true, SeqCst);
        }

        let mut h = DefaultHasher::new();
        HASH.load(SeqCst).hash(&mut h);
        self.0.hash(&mut h);
        write!(f, "{:016x}", h.finish())
    }
}
