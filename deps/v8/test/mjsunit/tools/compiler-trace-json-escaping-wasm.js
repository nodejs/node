// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --trace-turbo

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function compileWasmWithName(fnName) {
  const builder = new WasmModuleBuilder();
  builder.addFunction(fnName, kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
      .exportAs('f');
  const instance = builder.instantiate();
  assertEquals(2, instance.exports.f(1));
}

// Quotes, backslashes, newlines, and control characters in Wasm name section.
compileWasmWithName('bad"wasm\\name\n\x01');

// Multi-byte UTF-8 characters in Wasm name section and disassembly.
compileWasmWithName('caf\u00e9');
