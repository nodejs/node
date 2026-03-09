// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

use testutil::UiTestRunner;

#[test]
#[cfg_attr(miri, ignore)]
fn ui() {
    // This tests the behavior when `--cfg zerocopy_derive_union_into_bytes` is
    // present.
    UiTestRunner::new()
        .rustc_arg("--cfg=zerocopy_derive_union_into_bytes")
        .rustc_arg("--cfg=zerocopy_unstable_derive_on_error")
        .rustc_arg("-Wwarnings") // To ensure .stderr files reflect typical user encounter
        .run();

    // This tests the behavior when `--cfg zerocopy_derive_union_into_bytes` is
    // not present.
    UiTestRunner::new()
        .subdir("union_into_bytes_cfg")
        .rustc_arg("-Wwarnings") // To ensure .stderr files reflect typical user encounter
        .run();
}
