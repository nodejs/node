// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
const $obj0 = builder.addStruct({fields: [], descriptor: 1});
const $desc0 = builder.addStruct(
    {fields: [makeField(kWasmExternRef, false)], describes: $obj0});
builder.endRecGroup();

const $p0 = builder.addImportedGlobal('p', 'p0', kWasmExternRef);
const $p1 = builder.addImportedGlobal('p', 'p1', kWasmExternRef);
const $g_desc0 = builder.addGlobal(
    wasmRefType($desc0).exact(), false, false,
    [kExprGlobalGet, $p0, kGCPrefix, kExprStructNew, $desc0]);
const $g_desc1 = builder.addGlobal(
    wasmRefType($desc0).exact(), false, false,
    [kExprGlobalGet, $p1, kGCPrefix, kExprStructNew, $desc0]);

builder.addFunction('make0', makeSig([], [kWasmAnyRef])).exportFunc().addBody([
  kExprGlobalGet, $g_desc0.index,
  kGCPrefix, kExprStructNewDesc, $obj0,
]);
builder.addFunction('make1', makeSig([], [kWasmAnyRef])).exportFunc().addBody([
  kExprGlobalGet, $g_desc1.index,
  kGCPrefix, kExprStructNewDesc, $obj0,
]);

const kTrapSig = makeSig([kWasmExternRef, kWasmI32, kWasmExternRef], [kWasmI32]);
builder.addFunction('geti', kTrapSig).exportFunc().addBody([
  kExprI32Const, 11,
]);
builder.addFunction('geti2', kTrapSig).exportFunc().addBody([
  kExprI32Const, 22,
]);

const proto0 = {};
const proto1 = {};
const instance = builder.instantiate({p: {p0: proto0, p1: proto1}});
const wasm = instance.exports;

// Case 1: Proxy handler inherits the "get" trap from its prototype at an
// in-object offset beyond the handler's own instance size.
function HandlerProto() {
  this.a = 1;
  this.get = wasm.geti;
}
const handler_proto = new HandlerProto();
function EmptyHandler() {}
EmptyHandler.prototype = handler_proto;

// Finish slack tracking on EmptyHandler and make handler_proto fast.
function warmupProto(h) {
  return h.get;
}
for (let i = 0; i < 20; i++) {
  warmupProto(new EmptyHandler());
}

function allocHandlerAndAdjacentHeapNumber(v, zero) {
  const h = new EmptyHandler();
  const hn = v + zero;
  return [h, hn];
}
const [handler] = allocHandlerAndAdjacentHeapNumber(1.2, 0.0);

Object.setPrototypeOf(
    proto0, new Proxy(Object.freeze(Object.create(null)), handler));
Object.freeze(proto0);
const w0 = wasm.make0();

function readElem0(o) {
  return o[0];
}
%PrepareFunctionForOptimization(readElem0);
for (let i = 0; i < 5; i++) {
  assertEquals(11, readElem0(w0));
}
%OptimizeFunctionOnNextCall(readElem0);
assertEquals(11, readElem0(w0));

// Mutating the prototype trap or shadowing it on the handler must deoptimize.
handler_proto.get = wasm.geti2;
assertEquals(22, readElem0(w0));

%PrepareFunctionForOptimization(readElem0);
for (let i = 0; i < 5; i++) {
  assertEquals(22, readElem0(w0));
}
%OptimizeFunctionOnNextCall(readElem0);
assertEquals(22, readElem0(w0));

handler.get = () => 33;
assertEquals(33, readElem0(w0));

// Case 2: Proxy handler has an own constant "get" trap that is later mutated
// in-place.
const own_handler = {get: wasm.geti};
Object.setPrototypeOf(
    proto1, new Proxy(Object.freeze(Object.create(null)), own_handler));
Object.freeze(proto1);
const w1 = wasm.make1();

function readElem1(o) {
  return o[0];
}
%PrepareFunctionForOptimization(readElem1);
for (let i = 0; i < 5; i++) {
  assertEquals(11, readElem1(w1));
}
%OptimizeFunctionOnNextCall(readElem1);
assertEquals(11, readElem1(w1));

own_handler.get = wasm.geti2;
assertEquals(22, readElem1(w1));
