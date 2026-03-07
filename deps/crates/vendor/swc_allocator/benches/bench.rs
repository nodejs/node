extern crate swc_malloc;

use codspeed_criterion_compat::{black_box, criterion_group, criterion_main, Bencher, Criterion};
use swc_allocator::{boxed::Box as SwcBox, vec::Vec as SwcVec, Allocator, FastAlloc};

fn bench_alloc(c: &mut Criterion) {
    fn direct_alloc_std(b: &mut Bencher, times: usize) {
        b.iter(|| {
            let mut buf = std::vec::Vec::new();
            for i in 0..times {
                let item: std::boxed::Box<usize> = black_box(std::boxed::Box::new(black_box(i)));
                buf.push(item);
            }
        })
    }

    fn direct_alloc_no_scope(b: &mut Bencher, times: usize) {
        b.iter(|| {
            let mut vec = SwcVec::new();
            for i in 0..times {
                let item: SwcBox<usize> = black_box(SwcBox::new(black_box(i)));
                vec.push(item);
            }
        })
    }

    fn fast_alloc_no_scope(b: &mut Bencher, times: usize) {
        b.iter(|| {
            let alloc = FastAlloc::default();

            let mut vec = SwcVec::new_in(alloc);
            for i in 0..times {
                let item: SwcBox<usize> = black_box(SwcBox::new_in(black_box(i), alloc));
                vec.push(item);
            }
        })
    }

    fn direct_alloc_scoped(b: &mut Bencher, times: usize) {
        b.iter(|| {
            let allocator = Allocator::default();
            let _guard = unsafe { allocator.guard() };

            let mut vec = SwcVec::new();

            for i in 0..times {
                let item: SwcBox<usize> = black_box(SwcBox::new(black_box(i)));
                vec.push(item);
            }
        })
    }

    fn fast_alloc_scoped(b: &mut Bencher, times: usize) {
        b.iter(|| {
            let alloc = Allocator::default();
            let _guard = unsafe { alloc.guard() };

            let alloc = FastAlloc::default();

            let mut vec = SwcVec::new_in(alloc);

            for i in 0..times {
                let item: SwcBox<usize> = black_box(SwcBox::new_in(black_box(i), alloc));
                vec.push(item);
            }
        })
    }

    c.bench_function("common/allocator/alloc/std/1000000", |b| {
        direct_alloc_std(b, 1000000)
    });
    c.bench_function("common/allocator/alloc/no-scope/1000000", |b| {
        direct_alloc_no_scope(b, 1000000)
    });
    c.bench_function("common/allocator/alloc/scoped/1000000", |b| {
        direct_alloc_scoped(b, 1000000)
    });

    c.bench_function("common/allocator/alloc/cached-no-scope/1000000", |b| {
        fast_alloc_no_scope(b, 1000000)
    });
    c.bench_function("common/allocator/alloc/cached-scoped/1000000", |b| {
        fast_alloc_scoped(b, 1000000)
    });
}

criterion_group!(benches, bench_alloc);
criterion_main!(benches);
