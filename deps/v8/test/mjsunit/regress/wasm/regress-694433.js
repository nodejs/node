// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var size = Math.floor(0xFFFFFFFF / 4) + 1;
(function() {
  // Note: On 32 bit, this throws in the Uint16Array constructor (size does not
  // fit in a Smi). On 64 bit, WebAssembly.validate returns false, because the
  // size exceeds the internal module size limit.
  try {
    assertFalse(WebAssembly.validate(new Uint16Array(size)));
  } catch {
    assertThrows(() => new Uint16Array(size), RangeError);
  }
})();
gc();
