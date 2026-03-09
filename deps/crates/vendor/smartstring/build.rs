// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use version_check as rustc;

fn main() {
    let ac = autocfg::new();
    let has_feature = Some(true) == rustc::supports_feature("allocator_api");
    let has_api = ac.probe_trait("alloc::alloc::Allocator");
    if has_feature || has_api {
        autocfg::emit("has_allocator");
    }
    if has_feature {
        autocfg::emit("needs_allocator_feature");
    }
    autocfg::rerun_path("build.rs");
}
