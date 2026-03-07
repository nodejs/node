#![allow(clippy::unreadable_literal)]

use criterion::{criterion_group, criterion_main, Criterion};
use std::f64;
use std::hint;
use std::io::Write;

fn do_bench(c: &mut Criterion, group_name: &str, float: f64) {
    let mut group = c.benchmark_group(group_name);
    group.bench_function("zmij", |b| {
        let mut buf = zmij::Buffer::new();
        b.iter(move || {
            let float = hint::black_box(float);
            let formatted = buf.format_finite(float);
            hint::black_box(formatted);
        });
    });
    group.bench_function("ryu", |b| {
        let mut buf = ryu::Buffer::new();
        b.iter(move || {
            let float = hint::black_box(float);
            let formatted = buf.format_finite(float);
            hint::black_box(formatted);
        });
    });
    group.bench_function("std::fmt", |b| {
        let mut buf = Vec::with_capacity(20);
        b.iter(|| {
            buf.clear();
            let float = hint::black_box(float);
            write!(&mut buf, "{float}").unwrap();
            hint::black_box(buf.as_slice());
        });
    });
    group.finish();
}

fn bench(c: &mut Criterion) {
    do_bench(c, "f64[0]", 0f64);
    do_bench(c, "f64[short]", 0.1234f64);
    do_bench(c, "f64[medium]", 0.123456789f64);
    do_bench(c, "f64[e]", f64::consts::E);
    do_bench(c, "f64[max]", f64::MAX);
}

criterion_group!(benches, bench);
criterion_main!(benches);
