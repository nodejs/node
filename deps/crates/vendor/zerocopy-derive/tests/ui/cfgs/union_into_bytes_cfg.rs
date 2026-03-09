// Copyright 2024 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

//! See: https://github.com/google/zerocopy/issues/553
//! zerocopy must still allow derives of deprecated types.
//! This test has a hand-written impl of a deprecated type, and should result in a compilation
//! error. If zerocopy does not tack an allow(deprecated) annotation onto its impls, then this
//! test will fail because more than one compile error will be generated.
#![deny(deprecated)]

extern crate zerocopy_renamed;

use zerocopy_renamed::IntoBytes;

#[derive(IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union Foo {
    a: u8,
}

fn main() {}
