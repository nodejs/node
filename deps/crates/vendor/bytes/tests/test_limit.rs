#![warn(rust_2018_idioms)]

use bytes::{buf::Limit, BufMut};

#[test]
fn long_limit() {
    let buf = &mut [0u8; 10];
    let limit = buf.limit(100);
    assert_eq!(10, limit.remaining_mut());
    assert_eq!(&[0u8; 10], &limit.get_ref()[..]);
}

#[test]
fn limit_get_mut() {
    let buf = &mut [0u8; 128];
    let mut limit = buf.limit(10);
    assert_eq!(10, limit.remaining_mut());
    assert_eq!(&mut [0u8; 128], &limit.get_mut()[..]);
}

#[test]
fn limit_set_limit() {
    let buf = &mut [0u8; 128];
    let mut limit = buf.limit(10);
    assert_eq!(10, Limit::limit(&limit));
    limit.set_limit(5);
    assert_eq!(5, Limit::limit(&limit));
}

#[test]
fn limit_chunk_mut() {
    let buf = &mut [0u8; 20];
    let mut limit = buf.limit(10);
    assert_eq!(10, limit.chunk_mut().len());

    let buf = &mut [0u8; 10];
    let mut limit = buf.limit(20);
    assert_eq!(10, limit.chunk_mut().len());
}

#[test]
#[should_panic = "advance out of bounds"]
fn limit_advance_mut_panic_1() {
    let buf = &mut [0u8; 10];
    let mut limit = buf.limit(100);
    unsafe {
        limit.advance_mut(50);
    }
}

#[test]
#[should_panic = "cnt <= self.limit"]
fn limit_advance_mut_panic_2() {
    let buf = &mut [0u8; 100];
    let mut limit = buf.limit(10);
    unsafe {
        limit.advance_mut(50);
    }
}

#[test]
fn limit_advance_mut() {
    let buf = &mut [0u8; 100];
    let mut limit = buf.limit(10);
    unsafe {
        limit.advance_mut(5);
    }
    assert_eq!(5, limit.remaining_mut());
    assert_eq!(5, limit.chunk_mut().len());
}

#[test]
fn limit_into_inner() {
    let buf_arr = *b"hello world";
    let buf: &mut [u8] = &mut buf_arr.clone();
    let mut limit = buf.limit(4);
    let mut dst = vec![];

    unsafe {
        limit.advance_mut(2);
    }

    let buf = limit.into_inner();
    dst.put(&buf[..]);
    assert_eq!(*dst, b"llo world"[..]);
}
