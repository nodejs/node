// Copyright 2024 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

include!("../include.rs");

use util::{NotZerocopy, AU16};
use zerocopy::try_transmute_ref;

fn main() {}

fn ref_dst_mutable() {
    // `try_transmute_ref!` requires that its destination type be an immutable
    // reference.
    let _: Result<&mut u8, _> = try_transmute_ref!(&0u8);
    //~[msrv, stable, nightly]^ ERROR: mismatched types
    //~[msrv, stable, nightly]^^ ERROR: mismatched types

    // `try_transmute_ref` requires that the source type implements `Immutable`
    // and `IntoBytes`
    let dst_not_try_from_bytes: Result<&NotZerocopy, _> = try_transmute_ref!(&AU16(0));
    //~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy: TryFromBytes` is not satisfied
    //~[msrv, stable, nightly]^^ ERROR: the trait bound `NotZerocopy: TryFromBytes` is not satisfied
    //~[msrv, stable, nightly]^^^ ERROR: the trait bound `NotZerocopy: TryFromBytes` is not satisfied
    //~[msrv, stable, nightly]^^^^ ERROR: the trait bound `NotZerocopy: Immutable` is not satisfied

    // `try_transmute_ref` requires that the source type implements `Immutable`
    // and `IntoBytes`
    let src_not_into_bytes: Result<&AU16, _> = try_transmute_ref!(&NotZerocopy(AU16(0)));
    //~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy<AU16>: IntoBytes` is not satisfied
    //~[msrv, stable, nightly]^^ ERROR: the trait bound `NotZerocopy<AU16>: Immutable` is not satisfied
}
