// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-tier-mask-for-testing=2

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addMemory();
var sig_one = builder.addType(makeSig(new Array(9).fill(kWasmI32), []));
var zero = builder.addFunction('zero', kSig_v_i);
var one = builder.addFunction('one', sig_one);
var two = builder.addFunction('two', kSig_v_i);
// Function 0 ("zero"), compiled with Liftoff.
zero.addBody([kExprLocalGet, 0, kExprI32LoadMem, 0, 0, kExprDrop]);
// Function 1 ("one"), compiled with TurboFan.
one.addBody([kExprLocalGet, 7, kExprCallFunction, zero.index]);
// Function 2 ("two"), compiled with Liftoff.
two.addBody([
     kExprI32Const,     101,  // arg #0
     kExprI32Const,     102,  // arg #1
     kExprI32Const,     103,  // arg #2
     kExprI32Const,     104,  // arg #3
     kExprI32Const,     105,  // arg #4
     kExprI32Const,     106,  // arg #5
     kExprI32Const,     107,  // arg #6
     kExprI32Const,     108,  // arg #7
     kExprI32Const,     109,  // arg #8
     kExprCallFunction, one.index
   ])
    .exportFunc();
let instance = builder.instantiate();
assertTraps(kTrapMemOutOfBounds, instance.exports.two);
