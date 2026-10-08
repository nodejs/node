// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-concurrent-sparkplug --stack-size=100

const many_char_codes = new Array(11000).fill(48).join(",");
const loop_then_large_call = new Function(`
  for (let i = 0; i < 50; i++) {
    if (i === 49) return String.fromCharCode(${many_char_codes});
  }
`);

try {
  loop_then_large_call();
} catch (e) {
}
