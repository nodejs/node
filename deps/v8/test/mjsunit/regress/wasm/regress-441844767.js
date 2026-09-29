// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-in-js-inlining-body
// Flags: --turbolev --wasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("f", kSig_l_v).addBody([]).exportFunc();
assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
