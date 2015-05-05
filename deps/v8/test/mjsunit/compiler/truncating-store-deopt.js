// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g(a, b, c) {
  return a + b + c;
}

var asm = (function Module(global, env, buffer) {
  "use asm";

  var i32 = new global.Int32Array(buffer);

  // This is not valid asm.js, but we should still generate correct code.
  function store(x) {
    return g(1, i32[0] = x, 2);
  }

  return { store: store };
})({
  "Int32Array": Int32Array
}, {}, new ArrayBuffer(64 * 1024));

var o = { toString : function() { %DeoptimizeFunction(asm.store); return "1"; } }

asm.store(o);
