// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kOK = 11;
const kException = 42;

const kRefExtern = wasmRefType(kWasmExternRef);
const sigConcat = makeSig([kWasmExternRef, kWasmExternRef], [kRefExtern]);
const sigMain = makeSig([kWasmExternRef], [kWasmI32]);

const builder = new WasmModuleBuilder();
const concat = builder.addImport("wasm:js-string", "concat", sigConcat);

builder.addFunction("main", sigMain).exportFunc().addBody([
  kExprTry, kWasmI32,
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprCallFunction, concat,
    kExprDrop,
    kExprI32Const, kOK,
  kExprCatchAll,
    kExprI32Const, kException,
  kExprEnd,
]);

const main = builder.instantiate({}, { builtins: ["js-string"] }).exports.main;

assertEquals(kOK, main("a"));
const huge = "1234567890abcdef".repeat(%StringMaxLength() >> 4);
assertEquals(kException, main(huge));
%WasmTierUpFunction(main);
assertEquals(kOK, main("a"));
assertEquals(kException, main(huge));
