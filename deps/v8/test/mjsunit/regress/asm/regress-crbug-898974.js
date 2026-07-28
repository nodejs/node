// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Module(global, env, buffer) {
  "use asm";
  var HEAPF64 = new global.Float64Array(buffer);
  var HEAPF32 = new global.Float32Array(buffer);
  var Math_fround = global.Math.fround;
  function main_d_f() {
    HEAPF64[0] = Math_fround(+HEAPF64[0]);
  }
  function main_d_fq() {
    HEAPF64[1] = HEAPF32[4096];
  }
  function main_f_dq() {
    HEAPF32[4] = HEAPF64[4096];
  }
  return {main_d_f: main_d_f, main_d_fq: main_d_fq, main_f_dq: main_f_dq};
};
let buffer = new ArrayBuffer(4096);
let module = Module(this, undefined, buffer);
let view64 = new Float64Array(buffer);
let view32 = new Float32Array(buffer);
assertEquals(view64[0] = 2.3, view64[0]);
module.main_d_f();
module.main_d_fq();
module.main_f_dq();
assertTrue(%IsAsmWasmCode(Module));
assertEquals(Math.fround(2.3), view64[0]);
assertTrue(isNaN(view64[1]));
assertTrue(isNaN(view32[4]));
