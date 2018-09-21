// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

var global = {}

var fish = [
  {'name': 'foo'},
  {'name': 'bar'},
];

for (var i = 0; i < fish.length; i++) {
  global[fish[i].name] = 1;
}

function load() {
  var sum = 0;
  for (var i = 0; i < fish.length; i++) {
    var name = fish[i].name;
    sum += global[name];
  }
  return sum;
}

load();
load();
%OptimizeFunctionOnNextCall(load);
load();
assertOptimized(load);

function store() {
  for (var i = 0; i < fish.length; i++) {
    var name = fish[i].name;
    global[name] = 1;
  }
}

store();
store();
%OptimizeFunctionOnNextCall(store);
store();
assertOptimized(store);

// Regression test for KeyedStoreIC bug: would use PROPERTY mode erroneously.

function store_element(obj, key) {
  obj[key] = 0;
}

var o1 = new Array(3);
var o2 = new Array(3);
o2.o2 = "o2";
var o3 = new Array(3);
o3.o3 = "o3";
var o4 = new Array(3);
o4.o4 = "o4";
var o5 = new Array(3);
o5.o5 = "o5";
// Make the KeyedStoreIC megamorphic.
store_element(o1, 0);  // Premonomorphic
store_element(o1, 0);  // Monomorphic
store_element(o2, 0);  // 2-way polymorphic.
store_element(o3, 0);  // 3-way polymorphic.
store_element(o4, 0);  // 4-way polymorphic.
store_element(o5, 0);  // Megamorphic.

function inferrable_store(key) {
  store_element(o5, key);
}

inferrable_store(0);
inferrable_store(0);
%OptimizeFunctionOnNextCall(inferrable_store);
inferrable_store(0);
assertOptimized(inferrable_store);
// If |inferrable_store| emitted a generic keyed store, it won't deopt upon
// seeing a property name key. It should have inferred a receiver map and
// emitted an elements store, however.
inferrable_store("deopt");

// TurboFan is not sophisticated enough to use key type provided by ICs.
if (!isTurboFanned(inferrable_store)) {
  assertUnoptimized(inferrable_store);
}
