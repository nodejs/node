// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var boom = (function(stdlib, foreign, heap) {
  "use asm";
  var MEM8 = new stdlib.Uint8Array(heap);
  var MEM32 = new stdlib.Int32Array(heap);
  function foo(i, j) {
    j = MEM8[256];
    // This following value '10' determines the value of 'rax'
    MEM32[j >> 10] = 0xabcdefaa;
    return MEM32[j >> 2] + j
  }
  return foo
})(this, 0, new ArrayBuffer(256));
%PrepareFunctionForOptimization(boom);
%OptimizeFunctionOnNextCall(boom);
boom(0, 0x1000);
