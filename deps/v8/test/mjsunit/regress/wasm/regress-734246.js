// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addExplicitSection([
  kUnknownSectionCode,
  // section length
  0x0f,
  // name length: 0xffffffff
  0xf9, 0xff, 0xff, 0xff, 0x0f
]);
assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
