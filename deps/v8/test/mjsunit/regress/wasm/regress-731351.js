// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --expose-gc --stress-opt

gc();
function asm(stdlib, foreign, buffer) {
  "use asm";
  var HEAP32 = new stdlib.Uint32Array(buffer);
  function load(a) {
    a = a | 0;
    return +(HEAP32[a >> 2] >>> 0);
  }
  return {load: load};
}

function RunAsmJsTest() {
  buffer = new ArrayBuffer(65536);
  var asm_module = asm({Uint32Array: Uint32Array}, {}, buffer);
  asm_module.load(buffer.byteLength);
}
RunAsmJsTest();
