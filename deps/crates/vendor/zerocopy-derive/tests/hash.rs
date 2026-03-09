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

#[derive(imp::IntoBytes, imp::Immutable, imp::ByteHash)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct Struct {
    a: u64,
    b: u32,
    c: u32,
}

util_assert_impl_all!(Struct: imp::IntoBytes, imp::hash::Hash);

#[test]
fn test_hash() {
    use imp::{
        hash::{Hash, Hasher},
        DefaultHasher,
    };
    fn hash(val: impl Hash) -> u64 {
        let mut hasher = DefaultHasher::new();
        val.hash(&mut hasher);
        hasher.finish()
    }
    hash(Struct { a: 10, b: 15, c: 20 });
    hash(&[Struct { a: 10, b: 15, c: 20 }, Struct { a: 5, b: 4, c: 3 }]);
}
