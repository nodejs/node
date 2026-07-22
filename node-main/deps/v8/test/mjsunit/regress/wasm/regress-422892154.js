// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm-decoder

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $func0 = builder.addFunction(undefined, kSig_v_v).addBody([
    kExprUnreachable,
    kExprBrOnNull, 0,
    kExprDrop,
  ]);

builder.toModule();
