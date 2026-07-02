// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization

// Use a non-optimized function to process the iterated value to ensure we
// don't deopt for spurious reasons.
function process(results, x) {
  results.push(x);
}
%NeverOptimizeFunction(process);

function testForOf(iterable) {
  let results = [];
  for (let i of iterable) {
    process(results, i);
  }
  return results;
}

// Train with more than kMaxPolymorphicMapCount (4) distinct maps for the
// iterated object. Each array gets a unique property so its map is unique;
// V8's element-kind generalisation would otherwise collapse many element kinds
// into a common ancestor map before reaching the limit. Using holey doubles
// also causes the hole at index 1 to trigger CallIteratorNext (the slow path
// in ForOfNextHelper), which populates the call-slot with
// ArrayIteratorPrototypeNext so that TryReduceArrayIteratorForOfNext is
// reached at compile time. After 5 distinct maps both the GetIterator LoadIC
// and the iterated_object dummy LoadIC go megamorphic (empty maps vector).
// TryReduceArrayIteratorForOfNext must handle this gracefully (bail out via
// the maps.size() != 1 check) and let BuildForOfNextFallback handle
// execution. The function should stay Maglevved.
let a1 = [1.1, , 3.3]; a1.p1 = true;
let a2 = [1.1, , 3.3]; a2.p2 = true;
let a3 = [1.1, , 3.3]; a3.p3 = true;
let a4 = [1.1, , 3.3]; a4.p4 = true;
let a5 = [1.1, , 3.3]; a5.p5 = true;  // 5th distinct map -> megamorphic

%PrepareFunctionForOptimization(testForOf);
testForOf(a1);
testForOf(a2);
testForOf(a3);
testForOf(a4);
testForOf(a5);

%OptimizeMaglevOnNextCall(testForOf);
assertEquals([1.1, undefined, 3.3], testForOf(a1));

// Megamorphic iterated_object_feedback means TryReduceArrayIteratorForOfNext
// bails out and the generic fallback is used. No deopt expected.
assertTrue(isMaglevved(testForOf));

// Verify correct results via the fallback path.
assertEquals([1.1, undefined, 3.3], testForOf(a2));
assertEquals([1.1, undefined, 3.3], testForOf(a3));
assertTrue(isMaglevved(testForOf));
