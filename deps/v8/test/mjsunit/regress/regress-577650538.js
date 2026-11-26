// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Make initial_array_iterator_map unstable.
[1].values().foo = 1;

var arr_values = Array.prototype.values;

function readP(x) {
  // This field load is the same offset as the "iterated_object" of an array
  // iterator.
  return x.p;
}
%PrepareFunctionForOptimization(readP);

function readAndCall(fn, obj, doIt) {
  // When this is called from "trigger", doIt is always false,
  // so we never call fn (aka readP).

  // arr_values.call(obj.x) creates a VirtualObject for the array iterator.
  return doIt && fn(arr_values.call(obj.x));
}
%PrepareFunctionForOptimization(readAndCall);

function setX(obj, other, a) {
  obj.x = a + 1;
  other.extra = 1;
  return readP;
}
%PrepareFunctionForOptimization(setX);
%DisableEagerInlining(setX);

function trigger(obj, other, a, doIt) {
  // 1) Eagerly inline readAndCall
  // 2) Non-eagerly inline setX
  // 3) Non-eagerly inline fn (aka readP)
  let fn = setX(obj, other, a);
  return readAndCall(fn, obj, doIt);
}
%PrepareFunctionForOptimization(trigger);

function Obj() {
  this.x = "s";
}

let warmupObject = { x: { p: 1 } };
// Make x non const
warmupObject.x = { p: 2 };

readP(warmupObject.x);
// Make the fn call in readAndCall megamorphic. Also doIt = true so we actually
// call the function - but now the function is not readP (yet).
readAndCall(() => 1, warmupObject, true);
readAndCall(() => 2, warmupObject, true);

trigger(new Obj(), new Obj(), 0, false);

%OptimizeFunctionOnNextCall(trigger);
trigger(new Obj(), new Obj(), 10, false);
