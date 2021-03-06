// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

{
  let builder = new WasmModuleBuilder();
  builder.addMemory();
  builder.exportMemoryAs("exported_mem");
  i1 = builder.instantiate();
}
{
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("fil", "imported_mem");
  i2 = builder.instantiate({fil: {imported_mem: i1.exports.exported_mem}});
}
