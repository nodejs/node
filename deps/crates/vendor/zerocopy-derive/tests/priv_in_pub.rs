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

// FIXME(#847): Make this test succeed on earlier Rust versions.
#[::rustversion::stable(1.59)]
mod test {
    use super::*;

    // These derives do not result in E0446 as of Rust 1.59.0, because of
    // https://github.com/rust-lang/rust/pull/90586.
    //
    // This change eliminates one of the major downsides of emitting `where`
    // bounds for field types (i.e., the emission of E0446 for private field
    // types).

    #[derive(imp::KnownLayout, imp::IntoBytes, imp::FromZeros, imp::FromBytes, imp::Unaligned)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(C)]
    pub struct Public(Private);

    #[derive(imp::KnownLayout, imp::IntoBytes, imp::FromZeros, imp::FromBytes, imp::Unaligned)]
    #[zerocopy(crate = "zerocopy_renamed")]
    #[repr(C)]
    struct Private(());
}
