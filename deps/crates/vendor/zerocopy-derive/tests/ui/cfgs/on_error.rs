// Copyright 2026 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

extern crate zerocopy_renamed;

use zerocopy_renamed::FromBytes;

#[derive(FromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[zerocopy(on_error = "skip")]
#[repr(C)]
struct Foo(bool);

fn main() {}
