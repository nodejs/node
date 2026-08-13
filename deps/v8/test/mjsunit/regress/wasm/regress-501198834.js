// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addImport('m', '', kSig_i_v);
builder.addTable(kWasmFuncRef, 0);
try {
  builder.instantiate({m: {}});
} catch (e) {
  // Ignore LinkError to allow the test to complete and trigger heap verification
  // on the resulting unpublished objects.
}
