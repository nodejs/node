// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function makeGenerator() {
  return function* () {};
}

// %GeneratorPrototype% starts in fast-properties mode from the snapshot, with
// is_prototype_map() == true and should_be_fast_prototype_map() == false.
const generatorPrototype = Object.getPrototypeOf(makeGenerator()).prototype;
generatorPrototype.doubleProp = 1.5;

function foo(obj, gen) {
  // Access an in-object property first so TurboFan emits CheckMaps(obj).
  obj.next = 1;
  // Calling an uninitialized generator closure normalizes generatorPrototype
  // from fast properties (PropertyArray) to dictionary mode (NameDictionary).
  gen();
  // Because JSCreateGeneratorObject was marked kNoWrite, LoadElimination
  // eliminated the CheckMaps(obj) before this out-of-object double store.
  obj.doubleProp = 3.5;
}

// Warm up with two closures that already have a custom .prototype so that:
// 1. The call site in foo() is polymorphic over closures (not specialized to a
//    single HeapConstant closure) and lowers to JSCreateGeneratorObject.
// 2. Initializing their initial_map does not touch generatorPrototype.
const gen1 = makeGenerator();
const gen2 = makeGenerator();
gen1.prototype = {};
gen2.prototype = {};

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(gen1);
foo(generatorPrototype, gen1);
foo(generatorPrototype, gen2);

%OptimizeFunctionOnNextCall(foo);
const freshGen = makeGenerator();
foo(generatorPrototype, freshGen);
assertEquals(3.5, generatorPrototype.doubleProp);
