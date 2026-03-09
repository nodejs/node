// Copyright 2023 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

include!("../include.rs");

use zerocopy::transmute_mut;

fn main() {}

// `transmute_mut!` does not support transmuting into a non-reference
// destination type.
const DST_NOT_A_REFERENCE: usize = transmute_mut!(&mut 0u8);
//~[msrv, stable, nightly]^ ERROR: mismatched types
//~[msrv, stable, nightly]^^ ERROR: mismatched types

#[derive(zerocopy::FromBytes, zerocopy::IntoBytes, zerocopy::Immutable)]
#[repr(C)]
struct SrcA;

#[derive(zerocopy::IntoBytes, zerocopy::Immutable)]
#[repr(C)]
struct DstA;

// `transmute_mut` requires that the destination type implements `FromBytes`
const DST_NOT_FROM_BYTES: &mut DstA = transmute_mut!(&mut SrcA);
//~[msrv, stable, nightly]^ ERROR: the trait bound `DstA: FromBytes` is not satisfied

#[derive(zerocopy::FromBytes, zerocopy::IntoBytes, zerocopy::Immutable)]
#[repr(C)]
struct SrcB;

#[derive(zerocopy::FromBytes, zerocopy::Immutable)]
#[repr(C)]
struct DstB;

// `transmute_mut` requires that the destination type implements `IntoBytes`
const DST_NOT_AS_BYTES: &mut DstB = transmute_mut!(&mut SrcB);
//~[msrv, stable, nightly]^ ERROR: the trait bound `DstB: IntoBytes` is not satisfied

// `transmute_mut!` does not support transmuting between non-reference source
// and destination types.
const SRC_DST_NOT_REFERENCES: &mut usize = transmute_mut!(0usize);
//~[msrv, stable, nightly]^ ERROR: mismatched types

fn ref_src_immutable() {
    // `transmute_mut!` requires that its source type be a mutable reference.
    let _: &mut u8 = transmute_mut!(&0u8);
    //~[msrv, stable, nightly]^ ERROR: mismatched types
}

// `transmute_mut!` does not support transmuting from a non-reference source
// type.
const SRC_NOT_A_REFERENCE: &mut u8 = transmute_mut!(0usize);
//~[msrv, stable, nightly]^ ERROR: mismatched types

#[derive(zerocopy::IntoBytes, zerocopy::Immutable)]
#[repr(C)]
struct SrcC;

#[derive(zerocopy::FromBytes, zerocopy::IntoBytes, zerocopy::Immutable)]
#[repr(C)]
struct DstC;

// `transmute_mut` requires that the source type implements `FromBytes`
const SRC_NOT_FROM_BYTES: &mut DstC = transmute_mut!(&mut SrcC);
//~[msrv, stable, nightly]^ ERROR: the trait bound `SrcC: FromBytes` is not satisfied

#[derive(zerocopy::FromBytes, zerocopy::Immutable)]
#[repr(C)]
struct SrcD;

#[derive(zerocopy::FromBytes, zerocopy::IntoBytes, zerocopy::Immutable)]
#[repr(C)]
struct DstD;

// `transmute_mut` requires that the source type implements `IntoBytes`
const SRC_NOT_AS_BYTES: &mut DstD = transmute_mut!(&mut SrcD);
//~[msrv, stable, nightly]^ ERROR: the trait bound `SrcD: IntoBytes` is not satisfied

// `transmute_mut!` does not support transmuting from an unsized source type to
// a sized destination type.
const SRC_UNSIZED: &mut [u8; 1] = transmute_mut!(&mut [0u8][..]);
//~[msrv, stable]^ ERROR: the method `transmute_mut` exists for struct `Wrap<&mut [u8], &mut [u8; 1]>`, but its trait bounds were not satisfied
//~[nightly]^^ ERROR: transmute_mut` exists for struct `zerocopy::util::macro_util::Wrap<&mut [u8], &mut [u8; 1]>`, but its trait bounds were not satisfied
