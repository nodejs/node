// Copyright 2024 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// This file contains tests which trigger errors on MSRV during a different
// compiler pass compared to the stable or nightly toolchains.

#[macro_use]
extern crate zerocopy_renamed;

#[path = "../include.rs"]
mod util;

use zerocopy_renamed::IntoBytes;

use self::util::util::AU16;

fn main() {}

// `repr(C, packed(2))` is not equivalent to `repr(C, packed)`.
#[derive(IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed(2))]
struct IntoBytes1<T> {
    t0: T,
    // Add a second field to avoid triggering the "repr(C) struct with one
    // field" special case.
    t1: T,
}

fn is_into_bytes_1<T: IntoBytes>() {
    if false {
        is_into_bytes_1::<IntoBytes1<AU16>>();
        //~[msrv, stable, nightly]^ ERROR: the trait bound `AU16: zerocopy_renamed::Unaligned` is not satisfied
    }
}
