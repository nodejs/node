// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-asm --turbo-asm-deoptimization --allow-natives-syntax

function Module(heap) {
  "use asm";
  var a = new Uint8Array(heap);
  function f() {
    var x = a[0] | 0;
    %DeoptimizeFunction(f);
    return x;
  }
  return f;
}
assertEquals(0, Module(new ArrayBuffer(1))());
