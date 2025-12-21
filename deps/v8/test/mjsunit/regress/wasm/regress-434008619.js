// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-lazy-compilation --experimental-wasm-shared --wasm-staging

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $array0 = builder.addArray(kWasmI8, true, kNoSuperType, true);
builder.endRecGroup();
let $array7 = builder.addArray(kWasmI64, true, kNoSuperType, false);
let $sig21 = builder.addType(makeSig([kWasmExternRef], [wasmRefType($array0)]));
let encodeStringToUTF8Array = builder.addImport('wasm:text-encoder', 'encodeStringToUTF8Array', $sig21);
let $func18 = builder.addFunction(undefined, kSig_v_v);

$func18.addBody([
    kExprBlock, kWasmVoid,
      kExprTryTable, kWasmVoid, 1,
          kCatchAllNoRef, 0,
        kExprRefNull, kExternRefCode,
        kExprCallFunction, encodeStringToUTF8Array,
        kExprUnreachable,
      kExprEnd,
      kExprUnreachable,
    kExprEnd,
    kExprRefNull, $array7,
    ...wasmI32Const(5),
    ...wasmI64Const(32),
    kAtomicPrefix, kExprArrayAtomicOr, kAtomicAcqRel, $array7,
    kExprUnreachable,
  ]);

let kBuiltins = { builtins: ['js-string', 'text-decoder', 'text-encoder'] };
const instance = builder.instantiate({}, kBuiltins);
