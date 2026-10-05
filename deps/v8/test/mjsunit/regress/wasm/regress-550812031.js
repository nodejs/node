// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-generic-wrapper --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let imp_s = builder.addImport('mod', 'fn',
                              makeSig([], [wasmRefNullType(kWasmStructRef)]));

builder.addFunction('test', makeSig([], [wasmRefNullType(kWasmStructRef)]))
  .addBody([kExprCallFunction, imp_s])
  .exportFunc();

let js_fn = () => null;
let instance = builder.instantiate({ mod: { fn: js_fn } });
