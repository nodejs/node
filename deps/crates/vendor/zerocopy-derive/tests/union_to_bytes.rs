// Copyright 2019 The Fuchsia Authors
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

// A union is `imp::IntoBytes` if:
// - all fields are `imp::IntoBytes`
// - `repr(C)` or `repr(transparent)` and
//   - no padding (size of union equals size of each field type)
// - `repr(packed)`

#[derive(imp::IntoBytes, Clone, Copy)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union CZst {
    a: (),
}

util_assert_impl_all!(CZst: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
union C {
    a: u8,
    b: u8,
}

util_assert_impl_all!(C: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, align(2))]
union Aligned {
    a: [u8; 2],
}

util_assert_impl_all!(Aligned: imp::IntoBytes);

// Transparent unions are unstable; see issue #60405
// <https://github.com/rust-lang/rust/issues/60405> for more information.

// #[derive(imp::IntoBytes)]
// #[repr(transparent)]
// union Transparent {
//     a: u8,
//     b: CZst,
// }

// is_as_bytes!(Transparent);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
union CZstPacked {
    a: (),
}

util_assert_impl_all!(CZstPacked: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
union CPacked {
    a: u8,
    b: i8,
}

util_assert_impl_all!(CPacked: imp::IntoBytes);

#[derive(imp::IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C, packed)]
union CMultibytePacked {
    a: i32,
    b: u32,
    c: f32,
}

util_assert_impl_all!(CMultibytePacked: imp::IntoBytes);
