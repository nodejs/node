// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --allow-natives-syntax
// Flags: --experimental-wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/wasm/prototype-setup-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $obj0 = builder.addStruct({fields: [], descriptor: 1});
let $desc0 = builder.addStruct(
    {fields: [makeField(kWasmExternRef, false)], describes: $obj0});
builder.endRecGroup();

let proto_config = new WasmPrototypeSetupBuilder(builder);

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

const proto_imports = {};
const proto_globals = new Array(kNumDescriptors);
const constructor_funcs = new Array(kNumDescriptors);

for (let i = 0; i < kNumDescriptors; i++) {
  let name = `p${i}`;
  proto_globals[i] = builder.addImportedGlobal("p", name, kWasmExternRef);
  proto_imports[name] = {};
}
for (let i = 0; i < kNumDescriptors; i++) {
  let global =
      builder.addGlobal(wasmRefType($desc0).exact(), false, false, [
          kExprGlobalGet, proto_globals[i],
          kGCPrefix, kExprStructNew, $desc0]);
  let body = [
    kExprGlobalGet, global.index,
    kGCPrefix, kExprStructNewDesc, $obj0,
  ];
  constructor_funcs[i] =
      builder.addFunction("WasmP", $make_t).addBody(body);
}

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

for (let i of [0, 2, 3, 4, 5]) {
  proto_config.addConfig(proto_globals[i])
    .addConstructor(`W${i}`, constructor_funcs[i]);
}
proto_config.addConfig(proto_globals[1])
    .addConstructor("W1", constructor_funcs[1])
    .addMethod("foo", kWasmGetter, $foo);

proto_config.build();

let constructors = {};
let instance = builder.instantiate({
  p: proto_imports,
  c: {constructors},
}, {builtins: ["js-string", "js-prototypes"]});
let wasm = instance.exports;

Object.setPrototypeOf(
    constructors.W0.prototype,
    new Proxy(Object.freeze(Object.create(null)), Object.freeze({
      get: wasm.getp,
      set: wasm.setp,
    })));
Object.freeze(constructors.W0.prototype);

Object.setPrototypeOf(
    constructors.W1.prototype,
    new Proxy(Object.freeze(Object.create(null)), Object.freeze({
      get: wasm.getp,  // can't get inlined due to prototype getter {foo}.
      set: wasm.setf,  // can't get inlined due to prototype getter {foo}.
    })));
Object.freeze(constructors.W1.prototype);

Object.setPrototypeOf(
    constructors.W2.prototype,
    new Proxy(Object.freeze(Object.create(null)), Object.freeze({
      get: wasm.getp,
      set: wasm.setf,  // returns 0, meaning {false}.
    })));
Object.freeze(constructors.W2.prototype);

constructors.W3.prototype[0] = 42;
Object.setPrototypeOf(
    constructors.W3.prototype,
    new Proxy(Object.freeze(Object.create(null)), Object.freeze({
      get: wasm.geti,
      set: wasm.seti,
    })));
Object.freeze(constructors.W3.prototype);

Object.setPrototypeOf(
    constructors.W4.prototype,
    new Proxy(Array.prototype, Object.freeze({
      get: wasm.geti,
      set: wasm.seti,
    })));
Object.freeze(constructors.W4.prototype);

function W5Target() {}
let w5_target = new W5Target();
Object.setPrototypeOf(
    constructors.W5.prototype,
    new Proxy(w5_target, Object.freeze({
      get: wasm.geti,
      set: wasm.seti,
    })));
Object.freeze(constructors.W5.prototype);

let w0 = new constructors.W0();
let w1 = new constructors.W1();
let w2 = new constructors.W2();
let w3 = new constructors.W3();
let w4 = new constructors.W4();
let w5 = new constructors.W5();

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
