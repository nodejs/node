// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();

let sig = builder.addType(kSig_i_i);

let callee = builder.addFunction("callee", sig)
  .addBody([kExprLocalGet, 0])

let caller = builder.addFunction("caller", sig)
  .addBody([kExprLocalGet, 0, kExprRefFunc, callee.index, kExprCallRef, sig,
            kExprLocalGet, 0, kExprRefFunc, callee.index, kExprCallRef, sig,
            kExprI32Add])
  .exportFunc();

builder.addDeclarativeElementSegment([callee.index]);

builder.setCompilationPriority(caller.index, 0, 0);

builder.setInstructionFrequencies(caller.index,
  [{offset: 5, frequency: 32}, {offset: 11, frequency: 32}]);

// Both of these hints are out-of-bounds.
builder.setCallTargets(caller.index,
  [{offset: 5, targets: [{function_index: 10, frequency_percent: 100}]},
   {offset: 11, targets: [{function_index: 10, frequency_percent: 100}]}]);

let wasm = builder.instantiate().exports;

assertEquals(20, wasm.caller(10));
