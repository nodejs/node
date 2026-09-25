// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Also enable predictable mode. Otherwise, concurrent recompilation will be
// enabled, and the code generator will not try to print the InliningStack
// (see CodeGenerator::AssembleSourcePosition).
// These preconditions make this test quite fragile, but it's the only way
// currently to reproduce the crash.
// Flags: --code-comments --predictable --print-wasm-code

const builder = new WasmModuleBuilder();
// Add a call instruction, because the segfault happens when processing source
// positions.
builder.addFunction('foo', kSig_v_v).addBody([]);
builder.addFunction('test', kSig_v_v).addBody([kExprCallFunction, 0]);

builder.instantiate();
