#![cfg(feature = "nightly")]

use criterion::black_box;
use swc_allocator::{boxed::Box, Allocator, FastAlloc};

#[test]
fn direct_alloc_std() {
    let mut buf = std::vec::Vec::new();
    for i in 0..1000 {
        let item: std::boxed::Box<usize> = black_box(std::boxed::Box::new(black_box(i)));
        buf.push(item);
    }
}

#[test]
fn direct_alloc_no_scope() {
    let mut vec = swc_allocator::vec::Vec::new();
    for i in 0..1000 {
        let item: swc_allocator::boxed::Box<usize> =
            black_box(swc_allocator::boxed::Box::new(black_box(i)));
        vec.push(item);
    }
}

#[test]
fn direct_alloc_in_scope() {
    let allocator = Allocator::default();
    let _guard = unsafe { allocator.guard() };

    let mut vec = swc_allocator::vec::Vec::new();

    for i in 0..1000 {
        let item: swc_allocator::boxed::Box<usize> =
            black_box(swc_allocator::boxed::Box::new(black_box(i)));
        vec.push(item);
    }
}

#[test]
fn escape() {
    let allocator = Allocator::default();

    let obj = {
        let _guard = unsafe { allocator.guard() };
        Box::new(1234)
    };

    assert_eq!(*obj, 1234);
    // It should not segfault, because the allocator is still alive.
    drop(obj);
}

#[test]
fn global_allocator_escape() {
    let allocator = Allocator::default();
    let obj = {
        let _guard = unsafe { allocator.guard() };
        Box::new_in(1234, FastAlloc::global())
    };

    assert_eq!(*obj, 1234);
    drop(allocator);
    // Object created with global allocator should outlive the allocator.
    drop(obj);
}
