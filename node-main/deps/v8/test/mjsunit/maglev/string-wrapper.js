// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function create1() {
  return new String();
}

%PrepareFunctionForOptimization(create1);
create1();
%OptimizeMaglevOnNextCall(create1);
let s1 = create1();

assertTrue(s1 instanceof String);
assertEquals('object', typeof s1);
assertEquals('', s1.valueOf());

assertTrue(isMaglevved(create1));

function create2(v) {
  return new String(v);
}

%PrepareFunctionForOptimization(create2);
create2('hello');
%OptimizeMaglevOnNextCall(create2);
let s2 = create2('hello');

assertTrue(s2 instanceof String);
assertEquals('object', typeof s2);
assertEquals('hello', s2.valueOf());

let s3 = create2(42);
assertTrue(s3 instanceof String);
assertEquals('object', typeof s3);
assertEquals('42', s3.valueOf());

let sym = Symbol('private');
assertThrows(() => create2(sym));

let obj = {toString: () => {return 'stuff';}};
let s4 = create2(obj);
assertEquals('object', typeof s4);
assertEquals('stuff', s4.valueOf());

// No toString function here, but there's Object.prototype.toString.
let obj2 = {};
let s5 = create2(obj2);
%DebugPrint(s5);
assertEquals('object', typeof s5);
assertEquals('[object Object]', s5.valueOf());

assertTrue(isMaglevved(create2));

function create3() {
  return new String('constant');
}
%PrepareFunctionForOptimization(create3);
create3();
%OptimizeMaglevOnNextCall(create3);
let s6 = create3();
assertTrue(s6 instanceof String);
assertEquals('object', typeof s6);
assertEquals('constant', s6.valueOf());

function create4() {
  return new String(1983);
}
%PrepareFunctionForOptimization(create4);
create4();
%OptimizeMaglevOnNextCall(create4);
let s7 = create4();
assertTrue(s7 instanceof String);
assertEquals('object', typeof s7);
assertEquals('1983', s7.valueOf());
