// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kRefExtern = wasmRefType(kWasmExternRef);
const sigCast = makeSig([kWasmExternRef], [kRefExtern]);
const sigExternExtern = makeSig([kWasmExternRef], [kWasmExternRef]);
const sigI32Extern = makeSig([kWasmExternRef], [kWasmI32]);

const builder = new WasmModuleBuilder();
const cast = builder.addImport("wasm:js-string", "cast", sigCast);
const toLower = builder.addImport("m", "toLowerCase", sigExternExtern);
const length = builder.addImport("wasm:js-string", "length", sigI32Extern);

builder.addFunction("lower_len_catch", sigI32Extern).exportFunc().addBody([
  kExprTry, kWasmI32,
    kExprLocalGet, 0,
    kExprCallFunction, cast,
    kExprCallFunction, toLower,
    kExprCallFunction, length,
  kExprCatchAll,
    kExprI32Const, 42,
  kExprEnd,
]);

const instance = builder.instantiate({
  m: {toLowerCase: Function.prototype.call.bind(String.prototype.toLowerCase)},
}, {builtins: ["js-string"]});


const f = instance.exports.lower_len_catch;

assertEquals(3, f("ABC"));
assertEquals(2, f("\u0130"));
const huge = "\u0130".repeat(%StringMaxLength() / 2 + 1);
assertEquals(42, f(huge));
