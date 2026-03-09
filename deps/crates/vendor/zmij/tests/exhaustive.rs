#![cfg_attr(not(check_cfg), allow(unexpected_cfgs))]

use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;
use std::thread;

#[test]
#[cfg_attr(not(exhaustive), ignore = "requires cfg(exhaustive)")]
fn test_exhaustive() {
    const BATCH_SIZE: u32 = 1_000_000;
    let counter = Arc::new(AtomicU32::new(0));
    let finished = Arc::new(AtomicU32::new(0));

    let mut workers = Vec::new();
    for _ in 0..num_cpus::get() {
        let counter = counter.clone();
        let finished = finished.clone();
        workers.push(thread::spawn(move || loop {
            let batch = counter.fetch_add(1, Ordering::Relaxed);
            if batch > u32::MAX / BATCH_SIZE {
                return;
            }

            let min = batch * BATCH_SIZE;
            let max = if batch == u32::MAX / BATCH_SIZE {
                u32::MAX
            } else {
                min + BATCH_SIZE - 1
            };

            let mut zmij_buffer = zmij::Buffer::new();
            let mut ryu_buffer = ryu::Buffer::new();
            for u in min..=max {
                let f = f32::from_bits(u);
                if !f.is_finite() {
                    continue;
                }
                let zmij = zmij_buffer.format_finite(f);
                assert_eq!(Ok(f), zmij.parse());
                let ryu = ryu_buffer.format_finite(f);
                let matches = if ryu.contains('e') && !ryu.contains("e-") {
                    ryu.split_once('e') == zmij.split_once("e+")
                } else {
                    ryu == zmij
                };
                assert!(matches, "{ryu} != {zmij}");
            }

            let increment = max - min + 1;
            let update = finished.fetch_add(increment, Ordering::Relaxed);
            println!("{}", u64::from(update) + u64::from(increment));
        }));
    }

    for w in workers {
        w.join().unwrap();
    }
}
