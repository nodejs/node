// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.startRecGroup();
const kArrayI16 = builder.addArray(kWasmI16, {final: true});
builder.endRecGroup();

const kRefExtern = wasmRefType(kWasmExternRef);
const kFromCharCodeArray = builder.addImport(
    'wasm:js-string', 'fromCharCodeArray',
    makeSig([wasmRefNullType(kArrayI16), kWasmI32, kWasmI32], [kRefExtern]));

const kLength = %StringMaxLength() + 1;

builder.addFunction('main', kSig_r_v)
    .addBody([
      ...wasmI32Const(kLength),
      kGCPrefix, kExprArrayNewDefault, kArrayI16,
      ...wasmI32Const(0),
      ...wasmI32Const(kLength),
      kExprCallFunction, kFromCharCodeArray,
    ])
    .exportFunc();

const instance = builder.instantiate({}, {builtins: ['js-string']});
assertThrows(
    () => instance.exports.main(), RangeError, /Invalid string length/);
