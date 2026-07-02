// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle

// JS_BUNDLE_MODULE:b.mjs
export * from "a.mjs"

// JS_BUNDLE_MODULE_ENTRYPOINT
import {a} from "b.mjs"
assertEquals(a, 42);

// JS_BUNDLE_MODULE:a.mjs
export {a}
let a = 42;
