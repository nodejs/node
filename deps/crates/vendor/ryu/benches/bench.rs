use criterion::{criterion_group, criterion_main, Criterion};
use std::fmt::Display;
use std::hint;
use std::io::Write;
use std::{f32, f64};

fn do_bench(c: &mut Criterion, group_name: &str, float: impl ryu::Float + Display) {
    let mut group = c.benchmark_group(group_name);
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
    do_bench(c, "f64[e]", f64::consts::E);
    do_bench(c, "f64[max]", f64::MAX);

    do_bench(c, "f32[0]", 0f32);
    do_bench(c, "f32[short]", 0.1234f32);
    do_bench(c, "f32[e]", f32::consts::E);
    do_bench(c, "f32[max]", f32::MAX);
}

criterion_group!(benches, bench);
criterion_main!(benches);
