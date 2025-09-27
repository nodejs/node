// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --allow-natives-syntax
// Flags: --experimental-wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function StringToArray(str) {
  let result = [str.length];
  for (let c of str) result.push(c.charCodeAt(0));
  return result;
}

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $obj0 = builder.addStruct({fields: [], descriptor: 1});
let $desc0 = builder.addStruct({describes: $obj0});
builder.endRecGroup();

let kStringTest = builder.addImport('wasm:js-string', 'test', kSig_i_r);

let $make_t = builder.addType(makeSig([kWasmI32], [kWasmAnyRef]));

// We'll set up several descriptor/constructor pairs with different prototypes:
// 0: all good. Proxy handler is frozen.
// 1: Proxy traps can't get inlined due to existence of prototype method.
// 2: "set" trap returns {false}.
// 3: Proxy trap can't get inlined due to elements on the prototype chain.
// 4: Proxy handler is Array prototype, acquires elements later.
// 5: Proxy handler is empty object, acquires elements later.
const kNumDescriptors = 6;
const kFuncIndexOffset = 1;  // Because of {kStringTest}.
function MakeDescriptorAndConstructor(index) {
  let global =
      builder.addGlobal(wasmRefType($desc0).exact(), false, false, [
          kGCPrefix, kExprStructNewDefault, $desc0]);
  assertEquals(global.index, index);
  let body = [
    kExprGlobalGet, index,
    kGCPrefix, kExprStructNew, $obj0,
  ];
  let func = builder.addFunction("WasmP", $make_t).addBody(body);
  assertEquals(func.index, index + kFuncIndexOffset);
}
for (let i = 0; i < kNumDescriptors; i++) MakeDescriptorAndConstructor(i);

let $foo = builder.addFunction("foo", kSig_i_v).addBody([kExprI32Const, 42]);

let e_3 = new Array(3).fill(kWasmExternRef);
let e_4 = new Array(4).fill(kWasmExternRef);
let eie = [kWasmExternRef, kWasmI32, kWasmExternRef];
let eiee = [kWasmExternRef, kWasmI32, kWasmExternRef, kWasmExternRef];
builder.addFunction("getp", makeSig(e_3, [kWasmI32])).exportFunc().addBody([
  kExprLocalGet, 1,
  kExprCallFunction, kStringTest,
]);
builder.addFunction("setp", makeSig(e_4, [kWasmI32])).exportFunc().addBody([
  kExprLocalGet, 1,
  kExprCallFunction, kStringTest,
]);
builder.addFunction("setf", makeSig(e_4, [kWasmI32])).exportFunc().addBody([
  kExprI32Const, 0,
]);
builder.addFunction("geti", makeSig(eie, [kWasmI32])).exportFunc().addBody([
  kExprI32Const, 11,
]);
builder.addFunction("seti", makeSig(eiee, [kWasmI32])).exportFunc().addBody([
  kExprI32Const, 1,
]);


let global_entries = [
  kNumDescriptors,  // number of entries
];
function ConfigurePrototype(index, methods) {
  global_entries.push(...[
    index,  // global index
    0,  // no parent
    methods.length,
  ]);
  for (let m of methods) global_entries.push(...m);
  global_entries.push(...[
    1, ...StringToArray(`W${index}`), index + kFuncIndexOffset,  // constructor
    0,  // no statics
  ]);
}
ConfigurePrototype(0, []);
ConfigurePrototype(1, [[1, ...StringToArray("foo"), $foo.index]]);  // 1=getter
ConfigurePrototype(2, []);
ConfigurePrototype(3, []);
ConfigurePrototype(4, []);
ConfigurePrototype(5, []);

builder.addCustomSection("experimental-descriptors", [
  0,  // version
  0,  // module name
  2,  // "GlobalEntries" subsection
  ...wasmUnsignedLeb(global_entries.length),
  ...global_entries,
]);

let instance = builder.instantiate({}, {builtins: ["js-string"]});
let wasm = instance.exports;
Object.setPrototypeOf(
    wasm.W0.prototype,
    new Proxy(Object.freeze(Object.create(null)), Object.freeze({
      get: wasm.getp,
      set: wasm.setp,
    })));
Object.freeze(wasm.W0.prototype);

Object.setPrototypeOf(
    wasm.W1.prototype,
    new Proxy(Object.freeze(Object.create(null)), Object.freeze({
      get: wasm.getp,  // can't get inlined due to prototype getter {foo}.
      set: wasm.setf,  // can't get inlined due to prototype getter {foo}.
    })));
Object.freeze(wasm.W1.prototype);

Object.setPrototypeOf(
    wasm.W2.prototype,
    new Proxy(Object.freeze(Object.create(null)), Object.freeze({
      get: wasm.getp,
      set: wasm.setf,  // returns 0, meaning {false}.
    })));
Object.freeze(wasm.W2.prototype);

wasm.W3.prototype[0] = 42;
Object.setPrototypeOf(
    wasm.W3.prototype,
    new Proxy(Object.freeze(Object.create(null)), Object.freeze({
      get: wasm.geti,
      set: wasm.seti,
    })));
Object.freeze(wasm.W3.prototype);

Object.setPrototypeOf(
    wasm.W4.prototype,
    new Proxy(Array.prototype, Object.freeze({
      get: wasm.geti,
      set: wasm.seti,
    })));
Object.freeze(wasm.W4.prototype);

function W5Target() {}
let w5_target = new W5Target();
Object.setPrototypeOf(
    wasm.W5.prototype,
    new Proxy(w5_target, Object.freeze({
      get: wasm.geti,
      set: wasm.seti,
    })));
Object.freeze(wasm.W5.prototype);

let w0 = new wasm.W0();
let w1 = new wasm.W1();
let w2 = new wasm.W2();
let w3 = new wasm.W3();
let w4 = new wasm.W4();
let w5 = new wasm.W5();

class Test {
  constructor(body, expected, inner = null) {
    this.body = body;
    this.expected = expected;
    this.inner = inner;
  }
}

let tests = [

  new Test(function IntIsString() { return w0[0]; }, 1),

  new Test(function ObjectIsString() { return w0[{}]; }, 1),

  new Test(function StringIsString() {
    let sum = 0;
    for (let str of ["a", "bb", "ccc"]) {
      sum += w0[str];
    }
    return sum;
  }, 3),

  new Test(function NoMethodUsesProxy() {
    let result;
    for (let key of ["bar", "foo"]) result = w0[key];
    return result;
  }, 1),

  new Test(function MethodHidesProxy() {
    let result;
    for (let key of ["bar", "foo"]) result = w1[key];
    return result;
  }, 42),

  new Test(function StrictModeThrows(inner) {
    assertThrows(() => inner("foo"), TypeError,
                  /'set' on proxy: trap returned falsish/);
    assertThrows(() => inner("bar"), TypeError,
                  /'set' on proxy: trap returned falsish/);
    return 1;
  }, 1, function inner(key) {
    "use strict";
    w2[key] = 0;
  }),

  new Test(function StrictModeThrowsMethod(inner) {
    assertThrows(
      () => inner("foo"), TypeError,
      /Cannot set property foo of \[object Object\] which has only a getter/);
    assertThrows(
      () => inner("bar"), TypeError,
      /'set' on proxy: trap returned falsish/);
    return 1;
  }, 1, function inner(key) {
    "use strict";
    w1[key] = 0;
  }),

  new Test(function StrictModeCatches() {
    "use strict";
    let key = "foo";
    try {
      w2[key] = 0;
    } catch(e) {
      return "threw";
    }
    return "didn't throw";
  }, "threw"),

  new Test(function PrototypeElements() {
    "use strict";
    if (w3[0] !== 42) return `element 0 was wrong: ${w3[0]}`;
    if (w3[10] !== 11) return `element 10 was wrong: ${w3[10]}`;
    try {
      w3[10] = 0;  // Proxy trap, does not throw.
    } catch(e) {
      return "threw but shouldn't have"
    }
    try {
      w3[0] = 0;  // Read-only prototype element, throws.
    } catch(e) {
      return "threw as expected";
    }
    return "didn't throw";
  }, "threw as expected"),
];

for (let test of tests) {
  console.log(`Test: ${test.body.name}...`);
  %PrepareFunctionForOptimization(test.body);
  if (test.inner) { %PrepareFunctionForOptimization(test.inner); }
  for (let i = 0; i < 3; i++) {
    assertEquals(test.expected, test.body(test.inner));
  }
  %OptimizeFunctionOnNextCall(test.body);
  if (test.inner) { %OptimizeFunctionOnNextCall(test.inner); }
  assertEquals(test.expected, test.body(test.inner));
}

function GetW4(obj, index) { return obj[index]; }
function GetW5(obj, index) { return obj[index]; }
%PrepareFunctionForOptimization(GetW4);
%PrepareFunctionForOptimization(GetW5);
for (let i = 0; i < 3; i++) {
  assertEquals(11, GetW4(w4, 0));
  assertEquals(11, GetW4(w4, 10));
  assertEquals(11, GetW5(w5, 0));
  assertEquals(11, GetW5(w5, 10));
}
%OptimizeFunctionOnNextCall(GetW4);
%OptimizeFunctionOnNextCall(GetW5);
assertEquals(11, GetW4(w4, 0));
assertEquals(11, GetW4(w4, 10));
assertEquals(11, GetW5(w5, 0));
assertEquals(11, GetW5(w5, 10));
// Trigger deopt.
Object.defineProperty(
    Array.prototype, 0, {value: 999, configurable: false, writable: false});
assertThrows(() => GetW4(w4, 0), TypeError,
    /property '0' is a read-only and non-configurable data property/);
Object.defineProperty(
    w5_target, 0, {value: 999, configurable: false, writable: false});
assertThrows(() => GetW5(w5, 0), TypeError,
    /property '0' is a read-only and non-configurable data property/);
