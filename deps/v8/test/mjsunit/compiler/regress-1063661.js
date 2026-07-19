// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=1024

function main() {
  const v1 = [];
  for (let v11 = 0; v11 < 7; v11++) {
    for (let v16 = 0; v16 != 100; v16++) {}
    for (let v18 = -0.0; v18 < 7; v18 = v18 || 13.37) {
      const v21 = Math.max(-339,v18);
      v1.fill();
      undefined % v21;
    }
  }
}
main();
