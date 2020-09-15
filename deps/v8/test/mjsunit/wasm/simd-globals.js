// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd

load("test/mjsunit/wasm/wasm-module-builder.js");

// Test for S128 global with initialization.
// This checks for a bug in copying the immediate values from the
// initialization expression into the globals area of the module.
(function TestS128() {
  var builder = new WasmModuleBuilder();
  var g = builder.addGlobal(kWasmS128);
  g.init = [1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0];

  // Check that all lanes have the right values by creating 4 functions that
  // extract each lane.
  function addGetFunction(i) {
    builder.addFunction(`get${i}`, kSig_i_v)
      .addBody([
        kExprGlobalGet, g.index,
        kSimdPrefix, kExprI32x4ExtractLane, i])
      .exportAs(`get${i}`);
  }

  for (let i = 0; i < 4; i++) {
    addGetFunction(i);
  }

  var instance = builder.instantiate();

  for (let i = 0; i < 4; i++) {
    // get0 will get lane0, which has value 1
    assertEquals(i+1, instance.exports[`get${i}`]());
  }
})();
