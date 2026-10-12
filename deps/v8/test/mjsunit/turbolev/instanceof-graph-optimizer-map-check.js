// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Reducing a TestInstanceOf node in the graph optimizer emits a CheckMaps for
// the callable, and that check needs an eager deopt frame. TestInstanceOf used
// to carry lazy deopt info only, so there was no frame to attach to it.

function Ctor1() {}
function Ctor2() {}

// With a null prototype, looking up Symbol.hasInstance on the callable is a
// "not found" access, which is the path that checks the callable's map.
Object.setPrototypeOf(Ctor1, null);
Object.setPrototypeOf(Ctor2, null);

// Adding a property makes the constructors' map unstable, so the reduction has
// to emit an actual CheckMaps rather than just depending on a stable map.
Ctor1.x = 1;

function test(obj) {
  let result;
  // The callable is a loop phi rather than a constant, so the graph builder
  // emits a TestInstanceOf, which the graph optimizer then tries to reduce.
  for (let i = 0; i < 2; i++) {
    result = obj instanceof (i ? Ctor1 : Ctor2);
  }
  return result;
}

%PrepareFunctionForOptimization(test);
assertFalse(test({}));
%OptimizeFunctionOnNextCall(test);
assertFalse(test({}));
assertTrue(test(Object.create(Ctor1.prototype)));
