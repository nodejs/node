// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function Module(stdlib, env, heap) {
  "use asm";
  var MEM = new stdlib.Int32Array(heap);
  function f() {
    MEM[0] = 0;
  }
  return { f: f };
}
function instantiate() {
  var buffer = new ArrayBuffer(0);
  Module(this, {}, buffer).f();
  try {} finally {}
  gc();
  Module(this, {}, buffer).f();
}
instantiate();
assertTrue(%IsAsmWasmCode(Module));
