// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('foo', kSig_v_v).addBody([kExprDrop]);
assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
