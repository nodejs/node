// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const r = Realm.create();
const [scope, sub_scope] = Realm.eval(r, `
  var secret = 1234;
  var scope = Object.create(globalThis);
  var sub_scope = Object.create(scope);
  function rd(o) { return o.secret; }
  function has(o) { return 'secret' in o; }
  function wr(o, v) { o.secret = v; }
  function wr_new(o, v) { o.new_prop = v; }
  %PrepareFunctionForOptimization(rd);
  %PrepareFunctionForOptimization(has);
  %PrepareFunctionForOptimization(wr);
  %PrepareFunctionForOptimization(wr_new);
  for (var i = 0; i < 12; i++) {
    var s = {};
    s['p' + i] = 1;
    rd(s);
    has(s);
    wr(s, 1);
  }
  rd(scope);
  rd(sub_scope);
  has(scope);
  has(sub_scope);
  wr(Object.create(globalThis), 1234);
  wr(Object.create(scope), 1234);
  wr_new(Object.create(globalThis), 1);
  [scope, sub_scope];
`);

const g = Realm.global(r);
const current_realm = Realm.current();

function rd2(o) { return o.secret; }
function has2(o) { return 'secret' in o; }
function wr2(o, v) { o.secret = v; }
%PrepareFunctionForOptimization(rd2);
%PrepareFunctionForOptimization(has2);
%PrepareFunctionForOptimization(wr2);
for (let i = 0; i < 12; i++) {
  const s = {};
  s['p' + i] = 1;
  rd2(s);
  has2(s);
  wr2(s, 1);
}

// 1. Builtin Object.create with cross-realm prototype.
const o2 = Object.create(g);
assertEquals(current_realm, Realm.owner(o2));
assertThrows(() => rd2(o2), TypeError);
assertThrows(() => has2(o2), TypeError);
assertThrows(() => wr2(o2, 42), TypeError);

// 2. Optimized Object.create with constant cross-realm prototype
// (tests JSCreateLowering::ReduceJSCreateObject / JSObjectRef::GetObjectCreateMap).
function createWithConstantProto() {
  return Object.create(g);
}
%PrepareFunctionForOptimization(createWithConstantProto);
assertEquals(current_realm, Realm.owner(createWithConstantProto()));
%OptimizeFunctionOnNextCall(createWithConstantProto);
const o3 = createWithConstantProto();
assertEquals(current_realm, Realm.owner(o3));
assertThrows(() => rd2(o3), TypeError);

// 3. Optimized Object.create with dynamic cross-realm prototype
// (tests CreateObjectWithoutProperties in object.tq).
function createWithDynamicProto(proto) {
  return Object.create(proto);
}
%PrepareFunctionForOptimization(createWithDynamicProto);
assertEquals(current_realm, Realm.owner(createWithDynamicProto({})));
assertEquals(current_realm, Realm.owner(createWithDynamicProto(g)));
%OptimizeFunctionOnNextCall(createWithDynamicProto);
const o4 = createWithDynamicProto(g);
assertEquals(current_realm, Realm.owner(o4));
assertTrue(%HaveSameMap(o2, o3));
assertTrue(%HaveSameMap(o2, o4));
assertThrows(() => rd2(o4), TypeError);

// 4. Direct access to cross-realm objects with globalThis on their prototype
// chain (both direct and indirect prototype).
assertThrows(() => rd2(scope), TypeError);
assertThrows(() => rd2(sub_scope), TypeError);
assertThrows(() => has2(scope), TypeError);
assertThrows(() => has2(sub_scope), TypeError);
assertThrows(() => wr2(scope, 42), TypeError);
assertThrows(() => wr2(sub_scope, 42), TypeError);
