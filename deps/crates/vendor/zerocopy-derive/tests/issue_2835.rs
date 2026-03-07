// Copyright 2025 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

use zerocopy_renamed::{IntoBytes, Unalign};

#[allow(unused)]
#[derive(IntoBytes)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(C)]
struct Struct {
    leading: Unalign<u32>,
    trailing: [u8],
}

#[test]
fn test_issue_2835() {
    // Compilation is enough to verify the fix
}
