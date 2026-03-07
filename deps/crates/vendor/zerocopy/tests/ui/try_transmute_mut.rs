// Copyright 2024 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

include!("../include.rs");

use util::{NotZerocopy, AU16};
use zerocopy::try_transmute_mut;

fn main() {
    // `try_transmute_mut` requires that the destination type implements
    // `IntoBytes`
    let src = &mut AU16(0);
    let dst_not_try_from_bytes: Result<&mut NotZerocopy, _> = try_transmute_mut!(src);
    //~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy: TryFromBytes` is not satisfied
    //~[msrv, stable, nightly]^^ ERROR: the trait bound `NotZerocopy: TryFromBytes` is not satisfied
    //~[msrv, stable, nightly]^^^ ERROR: the trait bound `NotZerocopy: TryFromBytes` is not satisfied
    //~[msrv, stable, nightly]^^^^ ERROR: the trait bound `NotZerocopy: IntoBytes` is not satisfied

    #[derive(zerocopy::IntoBytes)]
    #[repr(C)]
    struct SrcA;

    #[derive(zerocopy::TryFromBytes)]
    #[repr(C)]
    struct DstA;

    // `try_transmute_mut` requires that the source type implements `FromBytes`
    let src_not_from_bytes: &mut DstA = try_transmute_mut!(&mut SrcA).unwrap();
    //~[msrv, stable, nightly]^ ERROR: the trait bound `SrcA: FromBytes` is not satisfied
    //~[msrv, stable, nightly]^^ ERROR: the trait bound `DstA: IntoBytes` is not satisfied

    #[derive(zerocopy::FromBytes)]
    #[repr(C)]
    struct SrcB;

    #[derive(zerocopy::TryFromBytes)]
    #[repr(C)]
    struct DstB;

    // `try_transmute_mut` requires that the source type implements `IntoBytes`
    let src_not_from_bytes: &mut DstB = try_transmute_mut!(&mut SrcB).unwrap();
    //~[msrv, stable, nightly]^ ERROR: the trait bound `SrcB: IntoBytes` is not satisfied
    //~[msrv, stable, nightly]^^ ERROR: the trait bound `DstB: IntoBytes` is not satisfied
}
