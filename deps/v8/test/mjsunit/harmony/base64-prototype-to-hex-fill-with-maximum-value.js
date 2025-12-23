// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64

(function TestBase64PrototypeToHexWithFillMaximumInInputs() {
  for (let i = 0; i < 100; ++i) {
    assertEquals(new Uint8Array(i).fill(255).toHex(), 'ff'.repeat(i));
  }
})();
