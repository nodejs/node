// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function makeGenerator() {
  return function* () {};
}

// %GeneratorPrototype% starts in fast-properties mode with a stable map.
const generatorPrototype = Object.getPrototypeOf(makeGenerator()).prototype;

function foo(gen) {
  // Calling an uninitialized generator closure normalizes generatorPrototype,
  // which invalidates its stable map and triggers lazy deoptimization of foo().
  gen();
  // Accessing generatorPrototype as a HeapConstant installs a compilation
  // dependency (DependOnStableMap) on generatorPrototype's map rather than
  // emitting a CheckMaps node.
  generatorPrototype.next = 1;
}

const gen1 = makeGenerator();
const gen2 = makeGenerator();
gen1.prototype = {};
gen2.prototype = {};

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(gen1);
foo(gen1);
foo(gen2);

%OptimizeFunctionOnNextCall(foo);
const freshGen = makeGenerator();
foo(freshGen);
assertEquals(1, generatorPrototype.next);
