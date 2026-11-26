// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle-reprl --allow-natives-syntax

// JS_BUNDLE_SCRIPT
print("part 1 before:", %NoElementsProtector());
Array.prototype[0] = 1;
print("part 1 after:", %NoElementsProtector());

// JS_BUNDLE_SCRIPT
print("part 2:", %NoElementsProtector());
