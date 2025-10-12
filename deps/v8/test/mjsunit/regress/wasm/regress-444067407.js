// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestIndexOutOfBounds() {
  let builder = new WasmModuleBuilder();

  builder.setCompilationPriority(0, 0, undefined);

  builder.instantiate();
})();
