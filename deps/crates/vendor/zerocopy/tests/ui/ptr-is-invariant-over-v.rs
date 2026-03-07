// Copyright 2025 The Fuchsia Authors
//
// Licensed under the 2-Clause BSD License <LICENSE-BSD or
// https://opensource.org/license/bsd-2-clause>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

include!("../include.rs");

use zerocopy::pointer::{
    invariant::{Aligned, Exclusive, Shared, Valid},
    Ptr,
};

fn _when_exclusive<'big: 'small, 'small>(
    big: Ptr<'small, &'big u32, (Exclusive, Aligned, Valid)>,
    mut _small: Ptr<'small, &'small u32, (Exclusive, Aligned, Valid)>,
) {
    _small = big;
    //~[msrv]^ ERROR: lifetime mismatch
    //~[stable, nightly]^^ ERROR: lifetime may not live long enough
}

fn _when_shared<'big: 'small, 'small>(
    big: Ptr<'small, &'big u32, (Shared, Aligned, Valid)>,
    mut _small: Ptr<'small, &'small u32, (Shared, Aligned, Valid)>,
) {
    _small = big;
    //~[msrv]^ ERROR: lifetime mismatch
    //~[stable, nightly]^^ ERROR: lifetime may not live long enough
}

fn main() {}
