// Copyright 2023 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

include!("../include.rs");

use zerocopy::transmute_ref;

use crate::util::AU16;

fn main() {}

fn ref_dst_mutable() {
    // `transmute_ref!` requires that its destination type be an immutable
    // reference.
    let _: &mut u8 = transmute_ref!(&0u8);
    //~[msrv, stable, nightly]^ ERROR: mismatched types
    //~[msrv, stable, nightly]^^ ERROR: mismatched types
    //~[msrv, stable, nightly]^^^ ERROR: mismatched types
    //~[msrv, stable, nightly]^^^^ ERROR: mismatched types
}

// `transmute_ref!` does not support transmuting into a non-reference
// destination type.
const DST_NOT_A_REFERENCE: usize = transmute_ref!(&0u8);
//~[msrv, stable, nightly]^ ERROR: mismatched types
//~[msrv, stable, nightly]^^ ERROR: mismatched types
//~[msrv, stable, nightly]^^^ ERROR: mismatched types
//~[msrv, stable, nightly]^^^^ ERROR: mismatched types

#[derive(zerocopy::Immutable)]
#[repr(transparent)]
struct DstA(AU16);

// `transmute_ref` requires that the destination type implements `FromBytes`
const DST_NOT_FROM_BYTES: &DstA = transmute_ref!(&AU16(0));
//~[msrv, stable, nightly]^ ERROR: the trait bound `DstA: FromBytes` is not satisfied

#[derive(zerocopy::FromBytes)]
#[repr(transparent)]
struct DstB(AU16);

// `transmute_ref` requires that the destination type implements `Immutable`
const DST_NOT_IMMUTABLE: &DstB = transmute_ref!(&AU16(0));
//~[msrv, stable, nightly]^ ERROR: the trait bound `DstB: Immutable` is not satisfied

// `transmute_ref!` does not support transmuting between non-reference source
// and destination types.
const SRC_DST_NOT_REFERENCES: usize = transmute_ref!(0usize);
//~[msrv, stable, nightly]^ ERROR: mismatched types
//~[msrv, stable, nightly]^^ ERROR: mismatched types
//~[msrv, stable, nightly]^^^ ERROR: mismatched types
//~[msrv, stable, nightly]^^^^ ERROR: mismatched types
//~[msrv, stable, nightly]^^^^^ ERROR: mismatched types

// `transmute_ref!` does not support transmuting from a non-reference source
// type.
const SRC_NOT_A_REFERENCE: &u8 = transmute_ref!(0usize);
//~[msrv, stable, nightly]^ ERROR: mismatched types

#[derive(zerocopy::Immutable)]
#[repr(transparent)]
struct SrcA(AU16);

// `transmute_ref` requires that the source type implements `IntoBytes`
const SRC_NOT_AS_BYTES: &AU16 = transmute_ref!(&SrcA(AU16(0)));
//~[msrv, stable, nightly]^ ERROR: the trait bound `SrcA: IntoBytes` is not satisfied
//~[msrv, stable, nightly]^^ ERROR: the trait bound `SrcA: IntoBytes` is not satisfied

#[derive(zerocopy::IntoBytes)]
#[repr(transparent)]
struct SrcB(AU16);

// `transmute_ref` requires that the source type implements `Immutable`
const SRC_NOT_IMMUTABLE: &AU16 = transmute_ref!(&SrcB(AU16(0)));
//~[msrv, stable, nightly]^ ERROR: the trait bound `SrcB: Immutable` is not satisfied
//~[msrv, stable, nightly]^^ ERROR: the trait bound `SrcB: Immutable` is not satisfied

// `transmute_ref!` does not support transmuting from an unsized source type.
const SRC_UNSIZED: &[u8; 1] = transmute_ref!(&[0u8][..]);
//~[msrv, stable]^ ERROR: the method `transmute_ref` exists for struct `Wrap<&[u8], &[u8; 1]>`, but its trait bounds were not satisfied
//~[nightly]^^ ERROR: the method `transmute_ref` exists for struct `zerocopy::util::macro_util::Wrap<&[u8], &[u8; 1]>`, but its trait bounds were not satisfied
