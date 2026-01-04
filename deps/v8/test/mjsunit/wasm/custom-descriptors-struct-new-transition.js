// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

// Test that struct.new and struct.new_default are still overloaded to take
// descriptor operands and allocate described types until users are able to
// update to use struct.new_desc and struct.new_default_desc instead.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $struct = builder.addStruct({descriptor: $desc});
/* $desc */ builder.addStruct({describes: $struct});
builder.endRecGroup();

builder.addGlobal(wasmRefType($struct), false, false, [
  kGCPrefix, kExprStructNewDefault, $desc,
  kGCPrefix, kExprStructNew, $struct]);

builder.addGlobal(wasmRefType($struct), false, false, [
  kGCPrefix, kExprStructNewDefault, $desc,
  kGCPrefix, kExprStructNewDefault, $struct]);

let instance = builder.instantiate();
