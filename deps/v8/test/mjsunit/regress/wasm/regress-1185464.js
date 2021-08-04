// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff --no-wasm-tier-up --wasm-tier-mask-for-testing=2
// Flags: --experimental-wasm-reftypes

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

// Generate a Liftoff call with too many reference parameters to fit in
// parameter registers, to force stack parameter slots.

const kManyParams = 32;
const kSigWithManyRefParams = makeSig(
  new Array(kManyParams).fill(kWasmExternRef), []);
const kPrepareManyParamsCallBody = Array.from(
  {length: kManyParams * 2},
  (item, index) => index % 2 == 0 ? kExprLocalGet : 0);


builder.addFunction(undefined, kSigWithManyRefParams).addBody([
]);

builder.addFunction(undefined, kSigWithManyRefParams)
.addBody([
  ...kPrepareManyParamsCallBody,
  kExprCallFunction, 0,  // call 0
]);

builder.addFunction(undefined, kSigWithManyRefParams).addBody([
  ...kPrepareManyParamsCallBody,
  kExprCallFunction,  1,  // call 1
]).exportAs('manyRefs');

const instance = builder.instantiate();
instance.exports.manyRefs();
