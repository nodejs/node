// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const struct_idx = builder.addStruct([
  makeField(kWasmI64, true),
  makeField(kWasmI32, false),
]);
builder.addFunction('make', makeSig([], [kWasmExternRef]))
    .addBody([
      kGCPrefix, kExprStructNewDefault, struct_idx,
      kGCPrefix, kExprExternConvertAny,
    ])
    .exportFunc();
builder.addFunction('setFirst', makeSig([kWasmExternRef, kWasmI64], []))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, struct_idx,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructSet, struct_idx, 0,
    ])
    .exportFunc();
builder.addFunction('getFirst', makeSig([kWasmExternRef], [kWasmI64]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, struct_idx,
      kGCPrefix, kExprStructGet, struct_idx, 0,
    ])
    .exportFunc();
builder.addFunction('getSecond', makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, struct_idx,
      kGCPrefix, kExprStructGet, struct_idx, 1,
    ])
    .exportFunc();

const wasm = builder.instantiate().exports;
const target = wasm.make();
const store = wasm.setFirst;

const iter = new Set([1]).values();
const next = iter.next.bind(iter);

function WithGetter(p) {
  this.p = p;
}
Object.defineProperty(WithGetter.prototype, 'y', {
  get() {
    const method = iter.next;
    const p = this.p;
    next();
    with (p) {}
    return 99;
  }
});

function WithField(p) {
  this.p = p;
  this.y = 1;
}

function getY(receiver) {
  return receiver.y;
}

function writeOrRead(write, value) {
  if (write) {
    store(target, value);
    return 0;
  }
  return getY(new WithField({q: 2}));
}

const getterObj = new WithGetter(new Uint8Array(1));
const fieldObj = new WithField({q: 2});

%PrepareFunctionForOptimization(getY);
%PrepareFunctionForOptimization(WithField);
%PrepareFunctionForOptimization(
    Object.getOwnPropertyDescriptor(WithGetter.prototype, 'y').get);
%PrepareFunctionForOptimization(writeOrRead);

getY(getterObj);
getY(getterObj);
getY(fieldObj);
writeOrRead(false, 0n);
writeOrRead(false, 0n);
writeOrRead(true, 0n);

%OptimizeFunctionOnNextCall(writeOrRead);
writeOrRead(true, 0x123456789abcdef0n);

assertEquals(0x123456789abcdef0n, wasm.getFirst(target));
assertEquals(0, wasm.getSecond(target));
