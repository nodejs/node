// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test should only be run on configurations that don't support Wasm SIMD.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test case manually reduced from https://crbug.com/1254675.
// This exercises a bug where we are missing checks for SIMD hardware support
// when a function has a v128 parameter but doesn't use any SIMD instructions.
(function() {
  const builder = new WasmModuleBuilder();
    builder.addType(kSig_i_s);
    builder.addFunction(undefined, 0)
            .addBodyWithEnd([kExprUnreachable, kExprEnd]);

  assertThrows(() => builder.instantiate());
}());

// Additional test case to verify that a declared v128 local traps.
(function() {
  const builder = new WasmModuleBuilder();
    builder.addType(kSig_i_i);
    builder.addFunction(undefined, 0)
           .addBodyWithEnd([kExprUnreachable, kExprEnd])
           .addLocals('v128', 1);

  assertThrows(() => builder.instantiate());
}());
