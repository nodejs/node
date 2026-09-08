// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle

// JS_BUNDLE_MODULE:A.mjs
import "./B.mjs";
await Promise.resolve();

// JS_BUNDLE_MODULE:B.mjs
import "./C.mjs";
await Promise.resolve();
throw new Error("async error in B");

// JS_BUNDLE_MODULE:C.mjs
import "./A.mjs";
await Promise.resolve();

// JS_BUNDLE_MODULE:X.mjs
import "./A.mjs";
await Promise.resolve();

// JS_BUNDLE_MODULE:compile.mjs
import * as nsB from "./B.mjs";
import * as nsX from "./X.mjs";

// JS_BUNDLE_MODULE_ENTRYPOINT:entry.mjs
let compile_failed = false;
let C_failed = false;
import("./compile.mjs").then(
  () => { fail("compile should have failed"); },
  (err) => {
    compile_failed = true;
    assertEquals(err.message, "async error in B");
    import("./C.mjs").then(
      () => { fail("C should have failed"); },
      (err2) => {
        C_failed = true;
        assertEquals(err2.message, "async error in B");
      }
    );
  }
);

setTimeout(() => {
  assertTrue(compile_failed);
  assertTrue(C_failed);
}, 10);
