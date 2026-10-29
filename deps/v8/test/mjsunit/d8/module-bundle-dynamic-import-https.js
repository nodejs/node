// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle --allow-natives-syntax

// JS_BUNDLE_MODULE:http://a.js
export const a = 1;
// JS_BUNDLE_SCRIPT
let gotModule = false;
import('http://a.js').then((module) => {
    assertEquals(1, module.a);
    gotModule = true;
});

%PerformMicrotaskCheckpoint();
assertTrue(gotModule);
