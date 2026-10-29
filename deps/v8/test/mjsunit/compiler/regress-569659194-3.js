// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function makeGenerator() {
  return function* () {};
}

// Even if %GeneratorPrototype% starts in dictionary mode or with
// should_be_fast_prototype_map() == true, iterating over a generator instance
// ensures %GeneratorPrototype% is in fast mode with
// should_be_fast_prototype_map() == true.
for (let _ in (function* () {})()) {}

const generatorPrototype = Object.getPrototypeOf(makeGenerator()).prototype;
generatorPrototype[0] = 1;

// Reconfiguring a field from Smi to Double on a detached prototype root map
// normalizes generatorPrototype to dictionary mode (NameDictionary) without
// clearing should_be_fast_prototype_map() on its PrototypeInfo.
generatorPrototype.x = 1;
generatorPrototype.x = 1.5;

function foo(obj, gen) {
  // Emit CheckMaps(obj, [dictionary_map]) before gen().
  obj[0];
  // Calling an uninitialized generator closure invokes
  // JSObject::OptimizeAsPrototype(generatorPrototype), which sees
  // should_be_fast_prototype_map() == true && !HasFastProperties() and calls
  // JSObject::MigrateSlowToFast(generatorPrototype), replacing its
  // NameDictionary with a PropertyArray.
  gen();
  // Without the fix, LoadElimination eliminated CheckMaps(obj, [dictionary_map])
  // before LoadDictionaryField, which then read the new PropertyArray as if it
  // were a NameDictionary.
  return obj.x;
}

const gen1 = makeGenerator();
const gen2 = makeGenerator();
gen1.prototype = {};
gen2.prototype = {};

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(gen1);
foo(generatorPrototype, gen1);
foo(generatorPrototype, gen2);

%OptimizeFunctionOnNextCall(foo);

// Populate dictionary properties (which does not change generatorPrototype's
// dictionary map) so that after MigrateSlowToFast packs all properties into a
// PropertyArray, every NameDictionary entry offset [6 + 3 * i] in the
// PropertyArray contains a valid fake dictionary entry ["x", "corrupted", 0].
for (let i = 0; i < 48; i += 3) {
  generatorPrototype['p' + i] = 'x';
  generatorPrototype['p' + (i + 1)] = 'corrupted';
  generatorPrototype['p' + (i + 2)] = 0;
}

const freshGen = makeGenerator();
assertEquals(1.5, foo(generatorPrototype, freshGen));
