// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();

builder.addExplicitSection([kFunctionSectionCode,
  // length
  7,
  // functions count
  1,
  // signature index (invalid LEB)
  0xff, 0xff, 0xff, 0xff, 0xff]);
builder.addExplicitSection([kStartSectionCode,
  // length
  1,
  // index
  0]);

assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
