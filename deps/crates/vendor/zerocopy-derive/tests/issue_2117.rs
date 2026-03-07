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
#![forbid(unexpected_cfgs)]

include!("include.rs");

// Make sure no unexpected `cfg`s are emitted by our derives (see #2117).

#[derive(imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
pub struct Test(pub [u8; 32]);
