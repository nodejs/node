// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let kRefExtern = wasmRefType(kWasmExternRef);

let builder = new WasmModuleBuilder();
kStringCast = builder.addImport(
  'wasm:js-string', 'cast', makeSig([kWasmExternRef], [kRefExtern]));
builder.addFunction("main", makeSig([], [kWasmExternRef]))
  .addBody([
    kExprI32Const, 1,
    kGCPrefix, kExprRefI31,
    kGCPrefix, kExprExternConvertAny,
    kExprCallFunction, kStringCast,
  ]).exportFunc();

let instance = builder.instantiate({}, {builtins: ["js-string"]});

assertTraps(kTrapIllegalCast, () => instance.exports.main());
