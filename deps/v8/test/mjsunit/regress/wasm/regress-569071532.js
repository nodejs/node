// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let sig0 = builder.addType(
    makeSig([kWasmI31Ref, kWasmExternRef], [kWasmExternRef]));
let sigCast =
    builder.addType(makeSig([kWasmExternRef], [wasmRefType(kWasmExternRef)]));
let jsStringCast = builder.addImport('wasm:js-string', 'cast', sigCast);

builder.addFunction('w0', sig0)
    .addBody([
      kExprLocalGet, 0,
      kExprRefIsNull,
      kExprI32Eqz,
      kExprIf, kWasmVoid,
        kExprLocalGet, 1,
        kGCPrefix, kExprBrOnCast, 0b11, 1, kExternRefCode, kExternRefCode,
        kExprDrop,
      kExprEnd,
      kExprLocalGet, 1,
      kExprLocalGet, 0,
      kExprBrOnNull, 0,
      kExprDrop,
      kExprLocalGet, 1,
      kExprBrOnNull, 0,
      kExprCallFunction, jsStringCast,
      kExprReturn,
    ])
    .exportFunc();

const instance = builder.instantiate({}, {builtins: ['js-string']});
assertEquals(42, instance.exports.w0(0, 42));
