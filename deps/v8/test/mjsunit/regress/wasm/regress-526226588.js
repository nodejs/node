// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(kSig_i_iii);
let $sig1 = builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], []));
let main = builder.addFunction(undefined, $sig0).exportAs('main');
let $func1 = builder.addFunction(undefined, $sig1).exportAs('func1');
let $mem0 = builder.addMemory(32, undefined);
let $table0 = builder.addTable(kWasmFuncRef, 2, 19);
let $segment0 = builder.addActiveElementSegment($table0.index, wasmI32Const(0), [[kExprRefFunc, main.index], [kExprRefFunc, $func1.index]], kWasmFuncRef);

main.addBody([
    kExprTry, kWasmI32,
            kExprI32Const, 1,
          ...wasmI32Const(0),
          ...wasmI32Const(0),
          kExprI32Const, 1,
          kExprCallIndirect, $sig1, $table0.index,
          ...wasmI32Const(0),
    kExprEnd,
  ]);

$func1.addBody([
    ...wasmI32Const(4590),
    ...wasmI32Const(-81),
    kAtomicPrefix, kExprI32AtomicSub8U, 64, 0, ...wasmUnsignedLeb(26478),
    kExprDrop,
  ]);

builder.exportMemoryAs("memory", 0);

const instance = builder.instantiate({});
print(instance.exports.main(1, 8471, 117676045));
let memory = new Int8Array(instance.exports.memory.buffer);
assertEquals(81, memory[0x795C]);
