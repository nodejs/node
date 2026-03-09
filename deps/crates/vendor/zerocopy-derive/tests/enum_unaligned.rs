// Copyright 2019 The Fuchsia Authors
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

// An enum is `Unaligned` if:
// - No `repr(align(N > 1))`
// - `repr(u8)` or `repr(i8)`

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum Foo {
    A,
}

util_assert_impl_all!(Foo: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i8)]
enum Bar {
    A,
}

util_assert_impl_all!(Bar: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8, align(1))]
enum Baz {
    A,
}

util_assert_impl_all!(Baz: imp::Unaligned);

#[derive(imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(i8, align(1))]
enum Blah {
    B,
}

util_assert_impl_all!(Blah: imp::Unaligned);
