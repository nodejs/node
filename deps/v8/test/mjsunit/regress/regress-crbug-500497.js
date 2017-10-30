// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// New space must be at max capacity to trigger pretenuring decision.
// Flags: --allow-natives-syntax --verify-heap --max-semi-space-size=1
// Flags: --expose-gc --no-always-opt

var global = [];  // Used to keep some objects alive.

function Ctor() {
  var result = {a: {}, b: {}, c: {}, d: {}, e: {}, f: {}, g: {}};
  return result;
}

gc();

for (var i = 0; i < 120; i++) {
  // Make the "a" property long-lived, while everything else is short-lived.
  global.push(Ctor().a);
  (function FillNewSpace() { new Array(10000); })();
}

// The bad situation is only triggered if Ctor wasn't optimized too early.
assertUnoptimized(Ctor);
// Optimized code for Ctor will pretenure the "a" property, so it will have
// three allocations:
// #1 Allocate the "result" object in new-space.
// #2 Allocate the object stored in the "a" property in old-space.
// #3 Allocate the objects for the "b" through "g" properties in new-space.
%OptimizeFunctionOnNextCall(Ctor);
for (var i = 0; i < 10000; i++) {
  // At least one of these calls will run out of new space. The bug is
  // triggered when it is allocation #3 that triggers GC.
  Ctor();
}
