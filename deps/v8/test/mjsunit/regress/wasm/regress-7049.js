// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

// Build two instances, instance 2 is interpreted, and calls instance 1 (via
// C_WASM_ENTRY), instance 1 then calls JS, which triggers GC.

let builder1 = new WasmModuleBuilder();

function call_gc() {
  print('Triggering GC.');
  gc();
  print('Survived GC.');
}
let func1_sig = makeSig(new Array(8).fill(kWasmI32), [kWasmI32]);
let imp = builder1.addImport('q', 'gc', kSig_v_v);
let func1 = builder1.addFunction('func1', func1_sig)
                .addBody([
                  kExprGetLocal, 0,  // -
                  kExprCallFunction, imp
                ])
                .exportFunc();
let instance1 = builder1.instantiate({q: {gc: call_gc}});

let builder2 = new WasmModuleBuilder();

let func1_imp = builder2.addImport('q', 'func1', func1_sig);
let func2 = builder2.addFunction('func2', kSig_i_i)
                .addBody([
                  kExprGetLocal, 0,  // 1
                  kExprGetLocal, 0,  // 2
                  kExprGetLocal, 0,  // 3
                  kExprGetLocal, 0,  // 4
                  kExprGetLocal, 0,  // 5
                  kExprGetLocal, 0,  // 6
                  kExprGetLocal, 0,  // 7
                  kExprGetLocal, 0,  // 8
                  kExprCallFunction, func1_imp
                ])
                .exportFunc();

let instance2 = builder2.instantiate({q: {func1: instance1.exports.func1}});

%RedirectToWasmInterpreter(
        instance2, parseInt(instance2.exports.func2.name));

// Call with 1. This will be passed by the C_WASM_ENTRY via the stack, and the
// GC will try to dereference it (before the bug fix).
instance2.exports.func2(1);
