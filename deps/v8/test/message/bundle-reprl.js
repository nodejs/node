// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle-reprl

// JS_BUNDLE_SCRIPT
let x = 1;
var y = 2;
globalThis.z = 3;
print("part 1:", x, y, z);

// JS_BUNDLE_SCRIPT
// Each part runs in a new context, so it can declare x again and it does not
// see the globals of part 1.
let x = 10;
print("part 2:", x, typeof y, typeof z);
throw new Error("part 2 throws");

// JS_BUNDLE_SCRIPT
// An exception in a part does not stop the next parts.
print("part 3:", typeof x);
