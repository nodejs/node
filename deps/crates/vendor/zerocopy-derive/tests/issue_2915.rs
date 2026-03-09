// Copyright 2026 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![no_implicit_prelude]
#![allow(dead_code, non_snake_case, non_camel_case_types, non_upper_case_globals)]

include!("include.rs");

// Original reproducer from #2915
#[derive(imp::TryFromBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(u8)]
enum MyEnum {
    Type,
    OtherWithData(u8),
}

// Tests for all other associated types which may be problematic in our codebase
// as of this commit.

#[derive(imp::FromBytes, imp::IntoBytes, imp::Unaligned, imp::Immutable, imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
pub struct StructCollision {
    pub Type: u8,
    pub LAYOUT: u8,
    pub PointerMetadata: u8,
    pub Tag: u8,
    pub tag: u8,
    pub variants: u8,
}

impl StructCollision {
    pub const Type: () = ();
    pub const LAYOUT: () = ();
    pub const PointerMetadata: () = ();
}

#[derive(imp::FromZeros, imp::Immutable, imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
pub enum EnumCollision {
    Type { x: u8 },
    LAYOUT { x: u8 },
    PointerMetadata { x: u8 },
    Tag { x: u8 },
    tag { x: u8 },
    variants { x: u8 },
}

// Case where generic parameter has associated type colliding with internal usage
pub trait AmbiguousTrait {
    type Type;
    const LAYOUT: usize;
    type PointerMetadata;
}

#[derive(imp::FromBytes, imp::IntoBytes, imp::Unaligned, imp::Immutable, imp::KnownLayout)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
pub struct GenericCollision<T: AmbiguousTrait> {
    pub field: u8,
    pub typ: T::Type,
    pub pointer_metadata: T::PointerMetadata,
    pub _marker: imp::PhantomData<T>,
}

#[test]
fn test_compilation() {
    // If the derives compile, we are good.
    let _ = StructCollision { Type: 0, LAYOUT: 0, PointerMetadata: 0, Tag: 0, tag: 0, variants: 0 };
    let _ = EnumCollision::Type { x: 0 };
}
