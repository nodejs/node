// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

for (let v0 = 0; v0 < 100; v0++) {
  for (let v2 = 0; v2 < 19; v2++) {
    Math["abs"](Math.max(4294967295, 0, v2, -0)) - v2;
  }
}
