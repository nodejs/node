// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test case 1 from crbug.com/460300660
try {
  v0 = %ConstructConsString("", "bbbbbbbbbbbbbbc");
} catch (e) {}
try {
  Boolean.__proto__[v0] = [];
} catch (e) {}
try {
  v0 = %ConstructConsString("", v0);
} catch (e) {}

// Test case 2 from crbug.com/460329223
const v1 = %ConstructThinString("Long enoµgh 2-byte string");
const v3 = %ConstructConsString(v1, "ConsStringConcatenation");
