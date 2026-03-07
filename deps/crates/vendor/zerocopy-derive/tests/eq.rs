// Copyright 2024 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// See comment in `include.rs` for why we disable the prelude.
#![no_implicit_prelude]
#![allow(warnings)]

include!("include.rs");

#[derive(imp::Debug, imp::IntoBytes, imp::Immutable, imp::ByteEq)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct Struct {
    a: u64,
    b: u32,
    c: u32,
}

util_assert_impl_all!(Struct: imp::IntoBytes, imp::PartialEq, imp::Eq);

#[test]
fn test_eq() {
    use imp::{assert_eq, assert_ne};
    let a = Struct { a: 10, b: 15, c: 20 };
    let b = Struct { a: 10, b: 15, c: 25 };
    assert_eq!(a, a);
    assert_ne!(a, b);
    assert_ne!(b, a);
}
