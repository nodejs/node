// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_v_v).addBody([
  kExprI32Const, 0, kExprBrTable,
  // 0x80000000 in LEB:
  0x80, 0x80, 0x80, 0x80, 0x08,
  // First break target. Creation of this node triggered the bug.
  0
]);
assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
