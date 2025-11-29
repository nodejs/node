// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

for (let v0 = 0; v0 < 100; v0++) {
  let v3 = Math.max([]);
  ++v3 / v3 in Math;
  for (let v7 = 0; v7 < 10; v7++) {
  }
}
