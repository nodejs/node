// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle

// JS_BUNDLE_SCRIPT
"use strict"

// JS_BUNDLE_SCRIPT
// This script is not strict mode, so this should pass:
function foo() {
  assertNotEquals(this, undefined);
}
foo();
