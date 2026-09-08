// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Tests for the compiler-inlined insertion sort of Array.prototype.sort.
// Both Maglev and Turbofan inline a small insertion sort for PACKED arrays
// with a provided comparefn and length <= 8.  Larger arrays fall through
// to the generic PowerSort builtin.  These tests are tier-neutral; the test
// runner variants exercise both compiler tiers.

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function opt(fn) {
  %PrepareFunctionForOptimization(fn);
  fn();
  %OptimizeFunctionOnNextCall(fn);
  fn();
}
%NeverOptimizeFunction(opt);

function optWith(fn, ...args) {
  %PrepareFunctionForOptimization(fn);
  fn(...args);
  %OptimizeFunctionOnNextCall(fn);
  return fn(...args);
}
%NeverOptimizeFunction(optWith);

// =========================================================================
// 1. PACKED_SMI_ELEMENTS — basic sorting
// =========================================================================

(function TestSmiAscending() {
  const cmp = (a, b) => a - b;
  function f() { return [5, 3, 1, 4, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5], f());
})();

(function TestSmiDescending() {
  const cmp = (a, b) => b - a;
  function f() { return [1, 2, 3, 4, 5].sort(cmp); }
  opt(f);
  assertEquals([5, 4, 3, 2, 1], f());
})();

(function TestSmiAlreadySorted() {
  const cmp = (a, b) => a - b;
  function f() { return [1, 2, 3, 4, 5, 6, 7, 8].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8], f());
})();

(function TestSmiReversed8() {
  const cmp = (a, b) => a - b;
  function f() { return [8, 7, 6, 5, 4, 3, 2, 1].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8], f());
})();

(function TestSmiAllEqual() {
  const cmp = (a, b) => a - b;
  function f() { return [7, 7, 7, 7, 7].sort(cmp); }
  opt(f);
  assertEquals([7, 7, 7, 7, 7], f());
})();

(function TestSmiDuplicates() {
  const cmp = (a, b) => a - b;
  function f() { return [3, 1, 4, 1, 5, 9, 2, 6].sort(cmp); }
  opt(f);
  assertEquals([1, 1, 2, 3, 4, 5, 6, 9], f());
})();

(function TestSmiNegative() {
  const cmp = (a, b) => a - b;
  function f() { return [-3, 0, 5, -1, 2].sort(cmp); }
  opt(f);
  assertEquals([-3, -1, 0, 2, 5], f());
})();

// =========================================================================
// 2. PACKED_DOUBLE_ELEMENTS
// =========================================================================

(function TestDoubleSort() {
  const cmp = (a, b) => a - b;
  function f() { return [5.5, 3.3, 1.1, 2.2].sort(cmp); }
  opt(f);
  assertEquals([1.1, 2.2, 3.3, 5.5], f());
})();

// =========================================================================
// 3. Boundary lengths
// =========================================================================

(function TestEmpty() {
  const cmp = (a, b) => a - b;
  function f() { return [].sort(cmp); }
  opt(f);
  assertEquals([], f());
})();

(function TestSingleElement() {
  const cmp = (a, b) => a - b;
  function f() { return [42].sort(cmp); }
  opt(f);
  assertEquals([42], f());
})();

(function TestTwoElements() {
  const cmp = (a, b) => a - b;
  function f() { return [2, 1].sort(cmp); }
  opt(f);
  assertEquals([1, 2], f());
})();

(function TestTwoElementsAlreadySorted() {
  const cmp = (a, b) => a - b;
  function f() { return [1, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2], f());
})();

(function TestThreeElementPermutations() {
  const cmp = (a, b) => a - b;
  function f1() { return [1, 2, 3].sort(cmp); }
  function f2() { return [1, 3, 2].sort(cmp); }
  function f3() { return [2, 1, 3].sort(cmp); }
  function f4() { return [2, 3, 1].sort(cmp); }
  function f5() { return [3, 1, 2].sort(cmp); }
  function f6() { return [3, 2, 1].sort(cmp); }
  for (const f of [f1, f2, f3, f4, f5, f6]) {
    opt(f);
    assertEquals([1, 2, 3], f());
  }
})();

// =========================================================================
// 4. Fast/slow path boundary (kMaxInlineSortSize = 8)
// =========================================================================

(function TestExactly8() {
  const cmp = (a, b) => a - b;
  function f() { return [8, 4, 6, 2, 7, 1, 5, 3].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8], f());
})();

(function TestSlowPath9() {
  const cmp = (a, b) => a - b;
  function f() { return [9, 8, 7, 6, 5, 4, 3, 2, 1].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], f());
})();

(function TestSlowPathLarge() {
  const cmp = (a, b) => a - b;
  function f() {
    const a = Array.from({length: 50}, (_, i) => 50 - i);
    a.sort(cmp);
    return a;
  }
  opt(f);
  assertEquals(Array.from({length: 50}, (_, i) => i + 1), f());
})();

// Mixed: optimized function sees both fast and slow path arrays at runtime.
(function TestMixedLengths() {
  const cmp = (a, b) => a - b;
  function f(arr) { return arr.sort(cmp); }
  optWith(f, [3, 1, 2]);
  assertEquals([1, 2, 3], f([3, 1, 2]));
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9],
               f([9, 8, 7, 6, 5, 4, 3, 2, 1]));
  assertEquals([1, 2, 3], f([3, 1, 2]));
})();

// =========================================================================
// 5. PACKED_ELEMENTS (tagged objects)
// =========================================================================

(function TestObjectsByProperty() {
  const cmp = (a, b) => a.v - b.v;
  function f() {
    return [{v: 3}, {v: 1}, {v: 2}].sort(cmp);
  }
  opt(f);
  assertEquals([1, 2, 3], f().map(x => x.v));
})();

(function TestStringsCustomComparator() {
  const cmp = (a, b) => a.length - b.length;
  function f() {
    return ["ccc", "a", "bb", "dddd", "ee"].sort(cmp);
  }
  opt(f);
  assertEquals(["a", "bb", "ee", "ccc", "dddd"], f());
})();

(function TestStringsLexicographic() {
  function cmp(a, b) { return a < b ? -1 : a > b ? 1 : 0; }
  %PrepareFunctionForOptimization(cmp);
  function f() {
    return ['cherry', 'apple', 'banana'].sort(cmp);
  }
  opt(f);
  assertEquals(['apple', 'banana', 'cherry'], f());
})();

// =========================================================================
// 6. sort() returns the same array object
// =========================================================================

(function TestReturnsSameArray() {
  const cmp = (a, b) => a - b;
  let arr;
  function f() {
    arr = [3, 1, 2];
    return arr.sort(cmp);
  }
  opt(f);
  assertTrue(f() === arr);
})();

(function TestSlowPathReturnsSameArray() {
  const cmp = (a, b) => a - b;
  let arr;
  function f() {
    arr = [9, 8, 7, 6, 5, 4, 3, 2, 1];
    return arr.sort(cmp);
  }
  opt(f);
  assertTrue(f() === arr);
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], arr);
})();

// =========================================================================
// 7. Comparefn edge cases
// =========================================================================

// comparefn always returns 0: stable sort preserves original order.
(function TestComparefnAlwaysZero() {
  const cmp = (a, b) => 0;
  function f() { return [5, 3, 1, 4, 2].sort(cmp); }
  opt(f);
  assertEquals([5, 3, 1, 4, 2], f());
})();

// comparefn returns fractional values.
(function TestComparefnFractional() {
  const cmp = (a, b) => (a - b) / 100.0;
  function f() { return [5, 3, 1, 4, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5], f());
})();

// comparefn returns very large / very small values.
(function TestComparefnLargeValues() {
  const cmp = (a, b) => (a < b) ? -1e15 : (a > b) ? 1e15 : 0;
  function f() { return [5, 3, 1, 4, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5], f());
})();

// comparefn that calls a helper function (tests inlining / call handling).
(function TestComparefnWithCalls() {
  function getRank(x) { return x; }
  %NeverOptimizeFunction(getRank);
  function cmp(a, b) { return getRank(a) - getRank(b); }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3], f());
})();

// comparefn with localeCompare (method call inside comparefn).
(function TestComparefnLocaleCompare() {
  function cmp(a, b) { return a.localeCompare(b); }
  %PrepareFunctionForOptimization(cmp);
  function f() { return ['cherry', 'apple', 'banana'].sort(cmp); }
  opt(f);
  assertEquals(['apple', 'banana', 'cherry'], f());
})();

// =========================================================================
// 8. Stability: insertion sort preserves relative order of equal keys.
// =========================================================================

(function TestStability() {
  const cmp = (a, b) => a.group - b.group;
  function f() {
    return [
      {group: 2, id: "a"},
      {group: 1, id: "b"},
      {group: 2, id: "c"},
      {group: 1, id: "d"},
      {group: 3, id: "e"},
    ].sort(cmp);
  }
  opt(f);
  const r = f();
  assertEquals([1, 1, 2, 2, 3], r.map(x => x.group));
  assertEquals(["b", "d", "a", "c", "e"], r.map(x => x.id));
})();

// =========================================================================
// 9. Snapshot semantics: comparefn side effects on the receiver should not
//    affect the sort result (temp array isolates the sort).
//    These tests may deopt; we only assert correctness, not tier.
// =========================================================================

(function TestComparefnModifiesElements() {
  const cmp = (a, b) => a - b;
  function f(arr) { return arr.sort(cmp); }
  optWith(f, [3, 1, 2]);
  // Now call with a mutating comparefn: copy-back overwrites mutations.
  let arr = [3, 1, 2];
  arr.sort((a, b) => { arr[0] = 99; return a - b; });
  assertEquals([1, 2, 3], arr);
})();

// =========================================================================
// 10. Length-change guard: comparefn that changes array length should
//     deopt and produce a valid result without crashing.
// =========================================================================

(function TestComparefnShrinksArray() {
  const cmp = (a, b) => a - b;
  function f(arr, c) { return arr.sort(c); }
  optWith(f, [3, 1, 2], cmp);
  let arr = [5, 3, 1, 4, 2];
  f(arr, (a, b) => { if (arr.length > 2) arr.length = 2; return a - b; });
  assertTrue(arr.length <= 5);
})();

(function TestComparefnGrowsArray() {
  const cmp = (a, b) => a - b;
  function f(arr, c) { return arr.sort(c); }
  optWith(f, [3, 1, 2], cmp);
  let arr = [5, 3, 1];
  let grew = false;
  f(arr, (a, b) => { if (!grew) { arr.push(99); grew = true; } return a - b; });
  // Must not crash.
})();

// =========================================================================
// 11. Comparefn `this` is undefined (called with ConvertReceiverMode::kAny).
// =========================================================================

(function TestComparefnThisIsUndefined() {
  "use strict";
  let observed_this;
  const cmp = function(a, b) { observed_this = this; return a - b; };
  function f() {
    observed_this = "sentinel";
    return [2, 1].sort(cmp);
  }
  opt(f);
  f();
  assertEquals(undefined, observed_this);
})();

// =========================================================================
// 12. Comparefn arguments are from the array.
// =========================================================================

(function TestComparefnArguments() {
  let recorded = [];
  const cmp = (a, b) => { recorded.push([a, b]); return a - b; };
  function f() {
    recorded = [];
    return [3, 1, 2].sort(cmp);
  }
  opt(f);
  f();
  for (const [a, b] of recorded) {
    assertTrue([1, 2, 3].includes(a), "a=" + a + " not in array");
    assertTrue([1, 2, 3].includes(b), "b=" + b + " not in array");
  }
})();

// =========================================================================
// 13. Repeated sorting of the same array.
// =========================================================================

(function TestRepeatedSort() {
  const cmp = (a, b) => a - b;
  let arr = [5, 3, 1, 4, 2];
  function f() { return arr.sort(cmp); }
  opt(f);
  for (let i = 0; i < 10; i++) {
    f();
    assertEquals([1, 2, 3, 4, 5], arr);
  }
})();

// =========================================================================
// 14. Comparefn that allocates (may trigger GC, moving elements pointers).
// =========================================================================

(function TestComparefnTriggersGC() {
  let garbage = [];
  const cmp = (a, b) => {
    garbage.push({x: a, y: b, z: new Array(100)});
    return a - b;
  };
  function f() {
    garbage = [];
    return [8, 4, 6, 2, 7, 1, 5, 3].sort(cmp);
  }
  opt(f);
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8], f());
})();

// =========================================================================
// 15. Comparefn result type: int32 / Smi / float64 paths.
// =========================================================================

// comparefn returns a raw int32 via a bitwise op.  Bitwise operations in
// Maglev always produce an untagged int32 node, so TryGetInt32 on the
// inlined return value succeeds directly (path 1, no Smi-untag needed).
(function TestComparefnReturnsInt32() {
  // (a - b) | 0 forces the result to be an untagged int32 in the graph.
  const cmp = (a, b) => (a - b) | 0;
  function f() { return [5, 3, 1, 4, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5], f());
})();

// comparefn returns -1/0/1 Smi literals.  Type feedback marks the return
// as NodeType::kSmi, exercising the UnsafeSmiUntag fast path.
(function TestComparefnReturnsSmi() {
  function cmp(a, b) { return a < b ? -1 : a > b ? 1 : 0; }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [5, 3, 1, 4, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3, 4, 5], f());
})();

// Same on a double array: elements are float64 but the comparefn result
// is still a Smi, so the Smi path is taken for the comparison.
(function TestComparefnReturnsSmiDoubleArray() {
  function cmp(a, b) { return a < b ? -1 : a > b ? 1 : 0; }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [5.5, 3.3, 1.1, 4.4, 2.2].sort(cmp); }
  opt(f);
  assertEquals([1.1, 2.2, 3.3, 4.4, 5.5], f());
})();

// comparefn returns NaN.  NaN is not representable as int32/Smi, so the
// float64 fallback is taken.  NaN < 0.0 is false, so NaN is treated as
// >= 0 (equal), and the stable sort preserves the original order.
(function TestComparefnReturnsNaN() {
  const cmp = () => NaN;
  function f() { return [5, 3, 1, 4, 2].sort(cmp); }
  opt(f);
  assertEquals([5, 3, 1, 4, 2], f());
})();

// comparefn returns float (fractional), exercising the float64 fallback
// on a double array.
(function TestComparefnReturnsFloatDoubleArray() {
  const cmp = (a, b) => (a - b) / 100.0;
  function f() { return [5.5, 3.3, 1.1, 4.4, 2.2].sort(cmp); }
  opt(f);
  assertEquals([1.1, 2.2, 3.3, 4.4, 5.5], f());
})();

// =========================================================================
// 16. Deopt mid-sort restarts correctly.
// =========================================================================

(function TestDeoptRestartsSort() {
  let triggerDeopt = false;
  function cmpDeopt(a, b) {
    if (triggerDeopt) %DeoptimizeFunction(sortDeopt);
    return a - b;
  }
  %PrepareFunctionForOptimization(cmpDeopt);
  function sortDeopt() { return [4, 2, 3, 1].sort(cmpDeopt); }
  opt(sortDeopt);
  assertEquals([1, 2, 3, 4], sortDeopt());

  // Trigger deopt: result must still be correct.
  triggerDeopt = true;
  assertEquals([1, 2, 3, 4], sortDeopt());
})();

// =========================================================================
// 17. Comparefn edge cases: undefined and non-callable.
// =========================================================================

// sort(undefined) is equivalent to sort() per spec (ECMA-262 §23.1.3.30
// step 1: "If comparefn is not undefined, ...").  Must NOT throw TypeError.
(function TestSortUndefinedComparefn() {
  function f() { return [10, 9, 2, 1].sort(undefined); }
  opt(f);
  // Default string comparison: "1" < "10" < "2" < "9".
  assertEquals([1, 10, 2, 9], f());
})();

// sort(nonCallable) must throw TypeError per spec, even from optimized code.
(function TestSortNonCallableComparefn() {
  let cmp = (a, b) => a - b;
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  cmp = 42;
  assertThrows(f, TypeError);
})();

// =========================================================================
// 18. Comparefn that throws.
// =========================================================================

// comparefn throws on first call from optimized code.
(function TestComparefnThrowsImmediately() {
  let shouldThrow = false;
  function cmp(a, b) {
    if (shouldThrow) throw new Error("boom");
    return a - b;
  }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3], f());
  shouldThrow = true;
  assertThrows(f, Error);
})();

// comparefn throws after a few successful comparisons.
(function TestComparefnThrowsLater() {
  let count = 0;
  let limit = Infinity;
  function cmp(a, b) {
    if (++count > limit) throw new RangeError("too many");
    return a - b;
  }
  %PrepareFunctionForOptimization(cmp);
  function f() {
    count = 0;
    return [5, 3, 1, 4, 2].sort(cmp);
  }
  opt(f);
  assertEquals([1, 2, 3, 4, 5], f());
  limit = 2;
  assertThrows(f, RangeError);
})();

// Non-inlined comparefn that throws (not folded to unconditional deopt).
(function TestComparefnThrowsNotInlined() {
  let shouldThrow = false;
  function cmp(a, b) {
    if (shouldThrow) throw new Error("boom");
    return a - b;
  }
  %PrepareFunctionForOptimization(cmp);
  %NeverOptimizeFunction(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3], f());
  shouldThrow = true;
  assertThrows(f, Error);
})();

// Non-inlined comparefn that throws after a few comparisons.
(function TestComparefnThrowsLaterNotInlined() {
  let count = 0;
  let limit = Infinity;
  function cmp(a, b) {
    if (++count > limit) throw new RangeError("too many");
    return a - b;
  }
  %PrepareFunctionForOptimization(cmp);
  %NeverOptimizeFunction(cmp);
  function f() {
    count = 0;
    return [5, 3, 1, 4, 2].sort(cmp);
  }
  opt(f);
  assertEquals([1, 2, 3, 4, 5], f());
  limit = 2;
  assertThrows(f, RangeError);
})();

// Throw from comparefn caught inside the sorting function — verify the
// exception is wired to the correct catch handler.  We also throw during
// feedback collection so that Maglev doesn't eliminate the catch handler.
(function TestComparefnThrowsWithCatchHandler() {
  let shouldThrow = false;
  function cmp(a, b) {
    if (shouldThrow) throw new Error("caught");
    return a - b;
  }
  %PrepareFunctionForOptimization(cmp);
  function f() {
    try {
      return [5, 3, 1, 4, 2].sort(cmp);
    } catch (e) {
      return e.message;
    }
  }
  // Collect feedback with a throw so the catch handler gets feedback.
  shouldThrow = true;
  %PrepareFunctionForOptimization(f);
  assertEquals("caught", f());
  shouldThrow = false;
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals([1, 2, 3, 4, 5], f());
  shouldThrow = true;
  assertEquals("caught", f());
})();

// =========================================================================
// 19. Comparefn returns non-number.
// =========================================================================

// comparefn returns a string — ToNumber conversion applies.
(function TestComparefnReturnsString() {
  function cmp(a, b) { return String(a - b); }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3], f());
})();

// comparefn returns undefined — treated as NaN, which is >= 0 (no swap).
(function TestComparefnReturnsUndefined() {
  function cmp(a, b) { return undefined; }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  // undefined -> NaN -> NaN < 0 is false -> stable (no swaps).
  assertEquals([3, 1, 2], f());
})();

// comparefn returns a boolean — true coerces to 1 (> 0), false to 0 (>= 0).
(function TestComparefnReturnsBoolean() {
  function cmp(a, b) { return a > b; }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  // true -> 1 -> swap; false -> 0 -> no swap.  Insertion sort on [3,1,2]:
  //   i=1: cmp(1,3) -> false(0) -> no swap -> [3,1,2]
  //   i=2: cmp(2,1) -> true(1) -> no swap; cmp(2,3) -> false(0) -> no swap
  //   Result depends on insertion sort specifics, just check no crash.
  assertEquals(3, f().length);
})();

// comparefn returns an object with valueOf — ToNumber applies.
(function TestComparefnReturnsObject() {
  function cmp(a, b) { return { valueOf() { return a - b; } }; }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);
  assertEquals([1, 2, 3], f());
})();

// =========================================================================
// 19. Non-inlined comparator (builtin / constructor / bound function)
// =========================================================================

// These exercise the path where the comparefn is callable but is NOT a
// user-defined JSFunction the inliner picks up — so the JSCall to comparefn
// stays generic.  All previous tests use `cmp = (a, b) => ...` which gets
// inlined; this section covers the alternative path that originally tripped
// crbug/502439789.

// Array constructor as comparator on PACKED_ELEMENTS.  Returns an array,
// which ToNumber coerces to NaN, so the sort order is unspecified — we just
// require that the call doesn't crash and the result has the right length.
(function TestPackedElementsBuiltinCmp() {
  function f() { return ['c', 'a', 'b'].sort(Array); }
  opt(f);
  assertEquals(3, f().length);
})();

// Same, with PACKED_SMI_ELEMENTS — verifies the SMI path doesn't crash even
// though it doesn't take the same TF reduction the bug originally hit.
(function TestPackedSmiBuiltinCmp() {
  function f() { return [3, 1, 2].sort(Array); }
  opt(f);
  assertEquals(3, f().length);
})();

// Math.max as comparator: returns a real number, so sort ordering is the
// max(a, b) of the two args (always >= 0), which causes a stable-ish order.
// Main goal: exercise SpeculativeToNumber on a Number return value with a
// non-inlined cmp.
(function TestMathMaxCmp() {
  function f() { return ['c', 'a', 'b'].sort(Math.max); }
  opt(f);
  assertEquals(3, f().length);
})();

// Length 0: copy loops have zero trip count.
(function TestEmptyArrayBuiltinCmp() {
  function f() { return [].sort(Array); }
  opt(f);
  assertEquals([], f());
})();

// Length exactly kMaxInlineSortLength (16): boundary of the fast path.
(function TestMaxInlineLengthBuiltinCmp() {
  const a = ['p', 'o', 'n', 'm', 'l', 'k', 'j', 'i',
             'h', 'g', 'f', 'e', 'd', 'c', 'b', 'a'];
  function f() { return a.slice().sort(Array); }
  opt(f);
  assertEquals(16, f().length);
})();

// Comparator that throws on first invocation: exercises the deferred
// throw / Unreachable continuation path that originally hit the DCHECK
// (the failing block in the regression-test trace was on this path).
(function TestThrowingCmp() {
  function badCmp() { throw new Error('boom'); }
  function f() {
    try {
      ['c', 'a', 'b'].sort(badCmp);
      return 'no-throw';
    } catch (e) {
      return e.message;
    }
  }
  opt(f);
  assertEquals('boom', f());
})();

// Comparator that mutates the receiver (length change): verifies the
// post-loop MaybeInsertMapChecks + CheckIf(length unchanged) deopt
// continuations.  The sort operates on a temp copy, so the mutation does
// NOT affect the sort order itself, but the post-sort length check should
// trip on the changed length.
(function TestReceiverMutatingCmp() {
  let arr;
  function mutate(a, b) { arr.length = 1; return 0; }
  function f() {
    arr = ['c', 'a', 'b'];
    arr.sort(mutate);
    return arr.length;
  }
  // Don't assert an exact length — the result depends on whether the
  // length-changed deopt fires before or after the mutating cmp call.
  // We just want to exercise the path without crashing.
  opt(f);
  f();
})();


// =========================================================================
// 20. Spec compliance: deopts during cmp must preserve permutation invariant
// =========================================================================
//
// ECMA-262 23.1.3.30: Array.prototype.sort takes a snapshot of the
// receiver's elements into _items_, sorts _items_ via comparefn, then
// writes _items_ back to the receiver.  Mutations of the receiver
// performed by comparefn are overwritten by the writeback, so the final
// array is always a permutation of the original elements.
//
// The inlined sort copies elements into a temp array before any cmp call.
// If a deopt fires during cmp, the deopt continuation must hand that
// snapshot to the generic sort -- re-reading the (possibly mutated)
// receiver would let cmp's side effects leak into the final result.

// Each test drives optimization with PrepareForOptimization+OptimizeOnNextCall
// on both `f` and `cmp` (so that cmp is inlined into f and the sort reduction
// fires), then triggers a deopt during cmp via the polymorphic-load technique
// or %DeoptimizeFunction.  All tests assert the ECMA-262 23.1.3.30 invariant:
// the final array is a permutation of the original elements.

// Polymorphic load: monomorphic on Smi map at warmup; flipping `mode` makes
// the IC miss, which is enough to deopt the surrounding optimized code.
const ic_obj_smi = { x: 1 };
const ic_obj_dbl = { x: 1.5 };
let deopt_mode = 0;
function readICX() {
  return (deopt_mode === 0 ? ic_obj_smi : ic_obj_dbl).x;
}

// Returns true iff `arr` is a permutation of `original` (multiset equal).
function isPermutation(arr, original) {
  if (arr.length !== original.length) return false;
  const a = arr.slice(), b = original.slice();
  a.sort((x, y) => (x < y ? -1 : x > y ? 1 : 0));
  b.sort((x, y) => (x < y ? -1 : x > y ? 1 : 0));
  for (let i = 0; i < a.length; ++i) if (a[i] !== b[i]) return false;
  return true;
}
%NeverOptimizeFunction(isPermutation);

// cmp mutates arr[0] on every call.  After flipping deopt_mode, the IC miss
// inside cmp's call to readICX deopts the outer optimized f mid-sort.  The
// final array must remain a permutation of the original; the mutation value
// must not appear in the result.
(function TestDeoptCmpKeepsPermutation() {
  let arr;
  function cmp(a, b) { arr[0] = 99; return a - b + readICX() - readICX(); }
  function f() {
    arr = [3, 1, 2];
    arr.sort(cmp);
    return arr;
  }
  %PrepareFunctionForOptimization(cmp);
  %PrepareFunctionForOptimization(f);
  deopt_mode = 0;
  f(); f();
  %OptimizeFunctionOnNextCall(f);
  %OptimizeFunctionOnNextCall(cmp);
  deopt_mode = 1;
  f();
  assertTrue(isPermutation(arr, [3, 1, 2]),
              "got " + JSON.stringify(arr));
  assertEquals(-1, arr.indexOf(99),
                "value 99 must not leak; got " + JSON.stringify(arr));
})();

// Worst-case reverse-sorted input forces every shift in the inner loop, so
// many cmp calls happen with temp_array mid-shift (pivot in register only).
// The deopt continuation must recover from any of these states.  The
// counter delays the polymorphic-load deopt to cmp call #N >= 3, so the
// deopt fires inside outer iter i=2's inner loop AFTER a shift has run
// (the "pivot-loss" state in the absence of the pivot-restore fix).
let pivot_loss_counter = 0;
(function TestDeoptCmpKeepsPermutationLong() {
  let arr;
  function cmp(a, b) {
    pivot_loss_counter++;
    arr[0] = 999;
    let k = 0;
    // Trigger the polymorphic-load deopt only on cmp call #3 onward.  For
    // input [5,4,3,2,1] the call schedule is:
    //   #1: iter i=1, j=0  (pre-shift)
    //   #2: iter i=2, j=1  (pre-shift in this outer iter)
    //   #3: iter i=2, j=0  (POST-shift -- pivot only in register pre-fix)
    if (pivot_loss_counter >= 3) k = readICX();
    return a - b + k - k;
  }
  function f() {
    pivot_loss_counter = 0;
    arr = [5, 4, 3, 2, 1];
    arr.sort(cmp);
    return arr;
  }
  %PrepareFunctionForOptimization(cmp);
  %PrepareFunctionForOptimization(f);
  deopt_mode = 0;
  f(); f();
  %OptimizeFunctionOnNextCall(f);
  %OptimizeFunctionOnNextCall(cmp);
  deopt_mode = 1;
  f();
  assertTrue(isPermutation(arr, [5, 4, 3, 2, 1]),
              "got " + JSON.stringify(arr));
  assertEquals(-1, arr.indexOf(999),
                "value 999 must not leak; got " + JSON.stringify(arr));
})();

// String value introduced by cmp must not appear in the result -- a leak
// is detectable by type alone (the original array contains only numbers).
(function TestDeoptCmpLeakedStringDoesNotAppear() {
  let arr;
  function cmp(a, b) {
    arr[1] = "INVADER";
    return a - b + readICX() - readICX();
  }
  function f() {
    arr = [3, 1, 2];
    arr.sort(cmp);
    return arr;
  }
  %PrepareFunctionForOptimization(cmp);
  %PrepareFunctionForOptimization(f);
  deopt_mode = 0;
  f(); f();
  %OptimizeFunctionOnNextCall(f);
  %OptimizeFunctionOnNextCall(cmp);
  deopt_mode = 1;
  f();
  for (let i = 0; i < arr.length; ++i) {
    assertEquals("number", typeof arr[i],
                  "non-number at " + i + ": " + JSON.stringify(arr));
  }
})();

// cmp grows the receiver mid-sort.  Positions [0, originalLength) must be a
// permutation of the snapshot; the pushed element may remain at its new
// position depending on when the deopt fires.
(function TestDeoptCmpGrowsReceiver() {
  let arr;
  function cmp(a, b) {
    if (arr.length === 3) arr.push(100);
    return a - b + readICX() - readICX();
  }
  function f() {
    arr = [3, 1, 2];
    arr.sort(cmp);
    return arr;
  }
  %PrepareFunctionForOptimization(cmp);
  %PrepareFunctionForOptimization(f);
  deopt_mode = 0;
  f(); f();
  %OptimizeFunctionOnNextCall(f);
  %OptimizeFunctionOnNextCall(cmp);
  deopt_mode = 1;
  f();
  assertTrue(isPermutation(arr.slice(0, 3), [3, 1, 2]),
              "positions 0..2 must be a permutation; got " +
              JSON.stringify(arr));
})();

// cmp shrinks the receiver mid-sort.  The spec captures len at the start
// of sort and writes that many elements back, re-extending the array if it
// was truncated.
(function TestDeoptCmpShrinksReceiver() {
  let arr;
  function cmp(a, b) {
    if (arr.length === 4) arr.length = 1;
    return a - b + readICX() - readICX();
  }
  function f() {
    arr = [4, 3, 2, 1];
    arr.sort(cmp);
    return arr;
  }
  %PrepareFunctionForOptimization(cmp);
  %PrepareFunctionForOptimization(f);
  deopt_mode = 0;
  f(); f();
  %OptimizeFunctionOnNextCall(f);
  %OptimizeFunctionOnNextCall(cmp);
  deopt_mode = 1;
  f();
  assertTrue(isPermutation(arr.slice(0, 4), [4, 3, 2, 1]),
              "positions 0..3 must be a permutation; got " +
              JSON.stringify(arr));
})();

// PACKED_ELEMENTS containing Undefined: CompareArrayElements
// (ECMA-262 23.1.3.30) requires Undefined values to sort to the end
// without invoking the comparator.  The inlined insertion sort cannot
// honor this special-case, so the reduction bails out at copy-in time
// when an Undefined is detected.  The first call after tier-up deopts;
// subsequent re-tiers do not re-inline the sort.
(function TestUndefinedInPackedElementsBailsOut() {
  let saw_undef_in_cmp = false;
  function cmp(a, b) {
    if (a === undefined || b === undefined) saw_undef_in_cmp = true;
    return (a < b) ? -1 : (a > b) ? 1 : 0;
  }
  function f(arr) {
    arr.sort(cmp);
    return arr;
  }
  %PrepareFunctionForOptimization(cmp);
  %PrepareFunctionForOptimization(f);
  f(["c", "x", "a", "b"]);
  f(["c", "x", "a", "b"]);
  %OptimizeFunctionOnNextCall(f);
  %OptimizeFunctionOnNextCall(cmp);
  f(["c", "x", "a", "b"]);
  assertOptimized(f);

  saw_undef_in_cmp = false;
  const arr = ["c", undefined, "a", "b"];
  const result = f(arr);
  assertEquals(4, result.length);
  assertEquals("a", result[0]);
  assertEquals("b", result[1]);
  assertEquals("c", result[2]);
  assertEquals(undefined, result[3]);
  assertFalse(saw_undef_in_cmp,
              "cmp must not be invoked with undefined arg");
  assertUnoptimized(f);

  // After the deopt the call IC is kDisallowSpeculation, so subsequent
  // re-tiers must not re-inline the sort and must not re-deopt.
  for (let i = 0; i < 3; ++i) {
    %PrepareFunctionForOptimization(f);
    %OptimizeFunctionOnNextCall(f);
    f(["c", undefined, "a", "b"]);
    assertOptimized(f, "f should stay optimized after retier #" + i);
  }
})();
