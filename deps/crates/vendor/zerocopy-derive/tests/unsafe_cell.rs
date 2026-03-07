// Copyright 2025 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

// See comment in `include.rs` for why we disable the prelude.
#![no_implicit_prelude]
#![allow(warnings)]

include!("include.rs");

// Test to make sure that all of our derives are compatible with `UnsafeCell`s.
//
// We test both `FromBytes` and `FromZeros`, as the `FromBytes` implied derive
// of `TryFromBytes` emits a trivial `is_bit_valid` impl - we want to test the
// non-trivial impl, which deriving `FromZeros` accomplishes.

#[derive(imp::FromBytes, imp::IntoBytes, imp::KnownLayout, imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct StructFromBytes(imp::UnsafeCell<u8>);

#[derive(imp::FromZeros, imp::IntoBytes, imp::KnownLayout, imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct StructFromZeros(imp::UnsafeCell<bool>);

#[derive(imp::FromZeros, imp::IntoBytes, imp::KnownLayout, imp::Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum EnumFromZeros {
    A(imp::UnsafeCell<bool>),
}
