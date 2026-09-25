// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64 --minor-ms

function test() {
  try {
    const input = test.toLocaleString();
    Uint8Array.fromBase64(input);
  } catch (e) {
  }
}
new Int8Array(268435439);
test();
