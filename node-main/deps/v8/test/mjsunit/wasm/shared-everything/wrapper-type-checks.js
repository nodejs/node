// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared
// Flags: --allow-natives-syntax --shared-string-table --harmony-struct

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let sharedJSStruct = new (new SharedStructType(["str"]));
sharedJSStruct.str = "shared string";
assertTrue(%IsSharedString(sharedJSStruct.str));

let createHeapNumber = (x) => x + x;

let vNull = () => null;
let vUndefined = () => undefined;
let vTrue = () => true;
let vFalse = () => false;
let vSmi = () => 1;
let vHeapNumber = () => 1.5;
let vHeapNumberSmi = () => createHeapNumber(5.5);
let vStringLiteral = () => "test";
let vStringDynamic = () => [1, 2, 3].join("-");
let vSharedString = () => sharedJSStruct.str;
let vSharedJSStruct = () => sharedJSStruct;
let vObject = () => ({object: "literal"});
let vSharedWasmStruct = (instance) => instance.exports.createSharedStruct();
let vSharedWasmArray = (instance) => instance.exports.createSharedArray();
let vUnsharedWasmStruct = (instance) => instance.exports.createUnsharedStruct();
let vUnsharedWasmArray = (instance) => instance.exports.createUnsharedArray();
let vSymbol = () => Symbol("symbol");

// List all values here, so they can't be forgotten in individual tests below.
let valuesToTest = [vNull, vUndefined, vTrue, vFalse, vSmi, vHeapNumber,
  vHeapNumberSmi, vStringLiteral, vStringDynamic, vSharedString, vObject,
  vSharedWasmStruct, vSharedWasmArray, vUnsharedWasmStruct, vUnsharedWasmArray,
  vSymbol];

let tests = [
  {
    name: "shared externref",
    type: wasmRefNullType(kWasmExternRef).shared(),
    valid: [vNull, vUndefined, vTrue, vFalse, vSmi, vStringLiteral,
      vSharedString, vSharedJSStruct, vSharedWasmStruct,
      vSharedWasmArray, vHeapNumber, vHeapNumberSmi, vStringDynamic],
    invalid: [vUnsharedWasmStruct, vUnsharedWasmArray, vObject, vSymbol]
  },
  {
    name: "shared anyref",
    type: wasmRefNullType(kWasmAnyRef).shared(),
    valid: [vNull, vUndefined, vTrue, vFalse, vSmi, vStringLiteral,
      vSharedString, vSharedJSStruct, vSharedWasmStruct,
      vSharedWasmArray, vHeapNumber, vHeapNumberSmi, vStringDynamic],
    invalid: [vUnsharedWasmStruct, vUnsharedWasmArray, vObject, vSymbol]
  },
  {
    name: "externref",
    type: wasmRefNullType(kWasmExternRef),
    valid: [vNull, vUndefined, vTrue, vFalse, vSmi, vHeapNumber, vStringLiteral,
      vStringDynamic, vObject, vUnsharedWasmStruct, vUnsharedWasmArray,
      vSharedString, vSharedJSStruct, vSharedWasmStruct, vSharedWasmArray,
      vHeapNumberSmi, vSymbol],
    invalid: []
  },
  {
    name: "anyref",
    type: wasmRefNullType(kWasmAnyRef),
    valid: [vNull, vUndefined, vTrue, vFalse, vSmi, vHeapNumber, vStringLiteral,
      vStringDynamic, vObject, vUnsharedWasmStruct, vUnsharedWasmArray,
      vSharedString, vSharedJSStruct, vSharedWasmStruct, vSharedWasmArray,
      vHeapNumberSmi, vSymbol],
    invalid: []
  },
  {
    name: "shared eqref",
    type: wasmRefNullType(kWasmEqRef).shared(),
    valid: [vNull, vSmi, vHeapNumberSmi, vSharedWasmStruct, vSharedWasmArray],
    invalid: [vUndefined, vTrue, vFalse, vHeapNumber, vStringLiteral,
      vStringDynamic, vSharedString, vSharedJSStruct, vObject,
      vUnsharedWasmStruct, vUnsharedWasmArray, vSymbol]
  },
  {
    name: "eqref",
    type: wasmRefNullType(kWasmEqRef),
    valid: [vNull, vSmi, vHeapNumberSmi, vUnsharedWasmStruct,
      vUnsharedWasmArray],
    invalid: [vUndefined, vTrue, vFalse, vHeapNumber, vStringLiteral,
      vStringDynamic, vSharedString, vSharedJSStruct, vObject,
      vSharedWasmStruct, vSharedWasmArray, vSymbol]
  },
  {
    name: "shared i31ref",
    type: wasmRefNullType(kWasmI31Ref).shared(),
    valid: [vNull, vSmi, vHeapNumberSmi],
    invalid: [vUndefined, vTrue, vFalse, vHeapNumber, vStringLiteral,
      vStringDynamic, vSharedString, vSharedJSStruct, vObject,
      vSharedWasmStruct, vSharedWasmArray, vUnsharedWasmStruct,
      vUnsharedWasmArray, vSymbol]
  },
  {
    name: "i31ref",
    type: wasmRefNullType(kWasmI31Ref),
    valid: [vNull, vSmi, vHeapNumberSmi],
    invalid: [vUndefined, vTrue, vFalse, vHeapNumber, vStringLiteral,
      vStringDynamic, vSharedString, vSharedJSStruct, vObject,
      vSharedWasmStruct, vSharedWasmArray, vUnsharedWasmStruct,
      vUnsharedWasmArray, vSymbol]
  },
  {
    name: "shared arrayref",
    type: wasmRefNullType(kWasmArrayRef).shared(),
    valid: [vNull, vSharedWasmArray],
    invalid: [vUndefined, vTrue, vFalse, vSmi, vHeapNumberSmi, vHeapNumber,
      vStringLiteral, vStringDynamic, vSharedString, vSharedJSStruct,
      vObject, vSharedWasmStruct, vUnsharedWasmStruct,
      vUnsharedWasmArray, vSymbol]
  },
  {
    name: "arrayref",
    type: wasmRefNullType(kWasmArrayRef),
    valid: [vNull, vUnsharedWasmArray],
    invalid: [vUndefined, vTrue, vFalse, vSmi, vHeapNumberSmi, vHeapNumber,
      vStringLiteral, vStringDynamic, vSharedString, vSharedJSStruct,
      vObject, vSharedWasmStruct, vUnsharedWasmStruct,
      vSharedWasmArray, vSymbol]
  },
  {
    name: "shared structref",
    type: wasmRefNullType(kWasmStructRef).shared(),
    valid: [vNull, vSharedWasmStruct],
    invalid: [vUndefined, vTrue, vFalse, vSmi, vHeapNumberSmi, vHeapNumber,
      vStringLiteral, vStringDynamic, vSharedString, vSharedJSStruct,
      vObject, vSharedWasmArray, vUnsharedWasmStruct,
      vUnsharedWasmArray, vSymbol]
  },
  {
    name: "structref",
    type: wasmRefNullType(kWasmStructRef),
    valid: [vNull, vUnsharedWasmStruct],
    invalid: [vUndefined, vTrue, vFalse, vSmi, vHeapNumberSmi, vHeapNumber,
      vStringLiteral, vStringDynamic, vSharedString, vSharedJSStruct,
      vObject, vSharedWasmStruct, vSharedWasmArray,
      vUnsharedWasmArray, vSymbol]
  },
  {
    name: "shared none",
    type: wasmRefNullType(kWasmNullRef).shared(),
    valid: [vNull],
    invalid: [vUndefined, vTrue, vFalse, vSmi, vHeapNumberSmi, vHeapNumber,
      vStringLiteral, vStringDynamic, vSharedString, vSharedJSStruct,
      vObject, vSharedWasmStruct, vSharedWasmArray, vUnsharedWasmStruct,
      vUnsharedWasmArray, vSymbol]
  },
  {
    name: "none",
    type: wasmRefNullType(kWasmNullRef),
    valid: [vNull],
    invalid: [vUndefined, vTrue, vFalse, vSmi, vHeapNumberSmi, vHeapNumber,
      vStringLiteral, vStringDynamic, vSharedString, vSharedJSStruct,
      vObject, vSharedWasmStruct, vSharedWasmArray, vUnsharedWasmStruct,
      vUnsharedWasmArray, vSymbol]
  },
];

for (const {name, type, valid, invalid} of tests) {
  print(`test ${name}`);
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct(
    [makeField(kWasmI32, true)], kNoSuperType, false, false);
  let sharedStruct = builder.addStruct(
    [makeField(kWasmI32, true)], kNoSuperType, false, true);
  let array = builder.addArray(kWasmI32, true, kNoSuperType, false, false);
  let sharedArray = builder.addArray(kWasmI32, true, kNoSuperType, false, true);

  builder.addFunction("createUnsharedStruct",
    makeSig([], [wasmRefType(struct)]))
  .addBody([
    kExprI32Const, 42,
    kGCPrefix, kExprStructNew, struct,
  ]).exportFunc();
  builder.addFunction("createSharedStruct",
    makeSig([], [wasmRefType(sharedStruct)]))
  .addBody([
    kExprI32Const, 42,
    kGCPrefix, kExprStructNew, sharedStruct,
  ]).exportFunc();
  builder.addFunction("createUnsharedArray",
    makeSig([], [wasmRefType(array)]))
  .addBody([
    kExprI32Const, 42,
    kGCPrefix, kExprArrayNewFixed, array, 1,
  ]).exportFunc();
  builder.addFunction("createSharedArray",
    makeSig([], [wasmRefType(sharedArray)]))
  .addBody([
    kExprI32Const, 42,
    kGCPrefix, kExprArrayNewFixed, sharedArray, 1,
  ]).exportFunc();

  builder.addFunction("consumer", makeSig([type], []))
    .addBody([]).exportFunc();

  let instance = builder.instantiate();

  for (const creator of valuesToTest) {
    if (valid.includes(creator)) {
      let value = creator(instance);
      print(`-- valid input ${creator.name}`);
      instance.exports.consumer(value);
    } else if (invalid.includes(creator)) {
      let value = creator(instance);
      print(`-- invalid input ${creator.name}`);
      assertThrows(() => instance.exports.consumer(value),
        TypeError, "type incompatibility when transforming from/to JS");
    } else {
      assertTrue(false, `${creator.name} neither listed as valid nor invalid`);
    }
  }
}
