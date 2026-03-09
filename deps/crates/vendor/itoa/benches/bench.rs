use criterion::{criterion_group, criterion_main, Criterion};
use std::fmt::Display;
use std::hint;
use std::io::Write;

fn do_bench(c: &mut Criterion, group_name: &str, int: impl itoa::Integer + Display) {
    let mut group = c.benchmark_group(group_name);
    group.bench_function("itoa", |b| {
        let mut buf = itoa::Buffer::new();
        b.iter(move || {
            let int = hint::black_box(int);
            let formatted = buf.format(int);
            hint::black_box(formatted);
        });
    });
    group.bench_function("std::fmt", |b| {
        let mut buf = Vec::with_capacity(20);
        b.iter(|| {
            buf.clear();
            let int = hint::black_box(int);
            write!(&mut buf, "{int}").unwrap();
            hint::black_box(buf.as_slice());
        });
    });
    group.finish();
}

fn bench(c: &mut Criterion) {
    do_bench(c, "u64[0]", 0u64);
    do_bench(c, "u64[half]", u64::from(u32::MAX));
    do_bench(c, "u64[max]", u64::MAX);

    do_bench(c, "i16[0]", 0i16);
    do_bench(c, "i16[min]", i16::MIN);

    do_bench(c, "u128[0]", 0u128);
    do_bench(c, "u128[max]", u128::MAX);
}

criterion_group!(benches, bench);
criterion_main!(benches);
