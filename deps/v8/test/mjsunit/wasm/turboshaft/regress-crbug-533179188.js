// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft-enable-debug-features --wasm-allow-mixed-eh-for-testing
// Flags: --no-liftoff --no-wasm-loop-unrolling --no-wasm-loop-peeling

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig1 = builder.addType(kSig_v_v);
let $struct4 = builder.addStruct({fields: [], final: true});
let $array6 = builder.addArray(kWasmI64);
let $sig9 = builder.addType(makeSig([kWasmF32], [kWasmI32]));
let $mem0 = builder.addMemory64(16, 17, true);
let $table0 = builder.addTable64(kWasmFuncRef, 21, undefined, [kExprRefNull, kNullFuncRefCode]);
let func_45_invoker = builder.addImport('dummy', 'func_45_invoker', $sig1);

let func_71_invoker = builder.addFunction(undefined, $sig1).exportAs('main')
  .addLocals(kWasmI32, 1)
  .addLocals(kWasmF32, 1)
  .addBody([
    ...wasmI64Const(-68719476736n),
    kGCPrefix, kExprStructNewDefault, $struct4,
    kExprBlock, kWasmRef, kExnRefCode,
      kExprTryTable, kWasmVoid, 1,
            kCatchAllRef, 0,
        kExprLocalGet, 0,
        kExprI32Eqz,
        kExprIf, kWasmVoid,
          kExprLoop, kWasmVoid,
            kExprBlock, kWasmVoid,
              kExprLoop, kWasmRef, $array6,
                kExprBlock, kWasmVoid,
                  kExprLocalGet, 0,
                  kExprIf, kWasmVoid,
                    kExprBlock, kWasmVoid,
                      ...wasmI32Const(608784195),
                      kExprI32Eqz,
                      kExprBrIf, 0,
                      kExprLocalGet, 0,
                      ...wasmI64Const(137438953472n),
                      ...wasmI32Const(-512),
                      kAtomicPrefix, kExprAtomicNotify, 2, 4,
                      kExprI32Add,
                      kExprI32Const, 25,
                      kGCPrefix, kExprArrayNewDefault, $array6,
                      kExprDrop,
                      kExprIf, kWasmVoid,
                        kExprUnreachable,
                      kExprEnd,
                    kExprEnd,
                  kExprElse,
                    kExprTryTable, kWasmI32, 1,
                          kCatchAllNoRef, 0,
                      kGCPrefix, kExprStructNewDefault, $struct4,
                      kExprLocalGet, 0,
                      kExprI32Eqz,
                      kExprIf, kWasmRef, kI31RefCode,
                        kExprTryTable, kWasmVoid, 1,
                              kCatchAllNoRef, 5,
                        kExprEnd,
                        kExprUnreachable,
                      kExprElse,
                        ...wasmI32Const(-64),
                        kGCPrefix, kExprRefI31,
                      kExprEnd,
                      kExprRefEq,
                    kExprEnd,
                    kExprLocalTee, 0,
                    kExprBrIf, 2,
                  kExprEnd,
                kExprEnd,
                kExprI32Const, 16,
                kGCPrefix, kExprArrayNewDefault, $array6,
              kExprEnd,
              kExprLocalGet, 0,
              kExprLocalGet, 1,
              kExprI64Const, 0,
              kExprCallIndirect, $sig9, $table0.index,
              kExprIf, kWasmRef, $struct4,
                kExprTryTable, kWasmRef, $struct4, 1,
                      kCatchAllNoRef, 2,
                  kExprCallFunction, func_45_invoker,
                  kGCPrefix, kExprStructNewDefault, $struct4,
                kExprEnd,
              kExprElse,
                kExprUnreachable,
              kExprEnd,
              kExprDrop, kExprDrop, kExprDrop,
            kExprEnd,
          kExprEnd,
        kExprElse,
          kExprReturn,
        kExprEnd,
      kExprEnd,
      kExprUnreachable,
    kExprEnd,
    kExprDrop, kExprDrop, kExprDrop,
  ]);

let dummy_imports = {dummy: {'func_45_invoker': () => {}}};
let buffer = builder.toBuffer();
let module = new WebAssembly.Module(buffer);
let instance = new WebAssembly.Instance(module, dummy_imports);

assertThrows(() => instance.exports.main(), WebAssembly.RuntimeError, /unreachable/);
