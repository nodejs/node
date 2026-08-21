// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap --track-array-buffer-views

const ab = new ArrayBuffer();
new Uint8Array(ab);
const byteLength = {
  valueOf() {
    gc();
    ab.transfer();
  }
};
try {
  new DataView(ab, 0, byteLength);
} catch (e) {}
