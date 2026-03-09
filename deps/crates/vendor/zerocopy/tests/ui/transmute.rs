// Copyright 2022 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

include!("../include.rs");

use util::{NotZerocopy, AU16};
use zerocopy::transmute;

fn main() {}

// `transmute` requires that the destination type implements `FromBytes`
const DST_NOT_FROM_BYTES: NotZerocopy = transmute!(AU16(0));
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy: FromBytes` is not satisfied

// `transmute` requires that the source type implements `IntoBytes`
const SRC_NOT_AS_BYTES: AU16 = transmute!(NotZerocopy(AU16(0)));
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy<AU16>: IntoBytes` is not satisfied
