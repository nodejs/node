// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code

function foo(x) {
  try {
    // 128MB + 1024 is not a multiple of 2^24, so asm.js linking will fail.
    // This size is small enough to reliably allocate on 32-bit platforms
    // but still fails validation on all platforms.
    var buffer = new ArrayBuffer((128 * 1024 * 1024) + 1024);
  } catch (e) {
    // Out of memory: soft pass.
    return;
  }

  (function Module(stdlib, foreign, heap) {
    "use asm";
    var MEM16 = new stdlib.Int16Array(heap);
    function load(i) {
      i = i | 0;
      i = MEM16[i >> 1] | 0;
      return i | 0;
    }
    return load;
  })(this, {}, buffer);
}

for (let i = 0; i < 3; i++) foo();
gc();
foo();
