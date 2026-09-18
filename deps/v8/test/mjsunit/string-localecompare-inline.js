// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Exercises the inlined String.prototype.localeCompare ASCII fast path that
// MachineLoweringReducer expands during MachineLoweringPhase, plus the
// bailout to the kStringFastLocaleCompare builtin.

const lc = String.prototype.localeCompare;

function cmp(a, b) { return a.localeCompare(b); }
%PrepareFunctionForOptimization(cmp);
cmp("a", "b");
cmp("b", "a");
cmp("hello", "hello");
%OptimizeFunctionOnNextCall(cmp);

function unopt(left, right) {
  // Path that does not go through the inlined reduction. Used as the source
  // of truth for cross-checking the optimized result.
  return left.localeCompare(right);
}

function check(left, right, expected_sign) {
  const actual = Math.sign(cmp(left, right));
  assertEquals(expected_sign, actual,
               `cmp(${JSON.stringify(left)}, ${JSON.stringify(right)})`);
  // Cross-check vs an un-inlined call when both inputs are strings.
  if (typeof left === "string" && typeof right === "string") {
    assertEquals(Math.sign(unopt(left, right)), actual,
                 "optimized vs unopt mismatch");
  }
}

// Pointer-equal early return.
const s = "Aircraft 7";
check(s, s, 0);
check("", "", 0);

// SeqOneByte fast path: differ on first byte.
check("Aircraft 0", "Aircraft 1", -1);
check("Aircraft 1", "Aircraft 0", +1);
check("apple", "banana", -1);
check("banana", "apple", +1);

// Equal prefix, differing length (bails to builtin).
check("a", "ab", -1);
check("ab", "a", +1);

// L1 collation tie (e.g. case differences) -- bails to builtin which
// performs the L3 tiebreak.
check("a", "A", -1);
check("A", "a", +1);
check("alpha", "Alpha", -1);

// Two-byte string (instance type guard bails).
check("héllo", "héllz", -1);
check("éé", "éé", 0);

// Cons string (instance type guard bails; equal case still 0).
const cons = "Air" + "craft 0";
check(cons, "Aircraft 0", 0);

// Non-string argument (smi guard bails to the builtin, which coerces).
check("5", 5, 0);

// Non-ASCII byte at byte-equal position (>=128 bail).
check("\xff", "\xff", 0);
check("a\xff", "a\xff", 0);

// --- Soundness: pointer-eq must not fire before the type checks.
//
// Without the gate, localeCompare.call(null, null) would silently return 0
// because the receiver and arg are tagged-equal as `null`. To make this
// regression test effective we need (a) the function to actually be
// optimized when the throwing call runs (otherwise a concurrent compile
// queue can leave us on the unoptimized path, which throws TypeError for an
// unrelated reason and the test passes spuriously), and (b) the call site
// to actually reach ReduceStringPrototypeLocaleCompareIntl. (b) relies on
// `lc` being a script-context constant so that ReduceFunctionPrototypeCall
// rewires `lc.call(x, y)` into a JSCall on lc itself; if that fold
// regresses the test will silently pass for the wrong reason. Hard to
// detect from JS alone -- the isOptimized check below at least guarantees
// (a) (and works in both --turbofan and --maglev --no-turbofan modes,
// unlike assertOptimized which gates on TF).
function callLC(left, right) {
  return lc.call(left, right);
}
%PrepareFunctionForOptimization(callLC);
callLC("a", "b");
callLC("c", "d");
%OptimizeFunctionOnNextCall(callLC);
callLC("e", "f");
assertTrue(isOptimized(callLC), "callLC should be optimized");

assertThrows(() => callLC(null, null), TypeError);
assertTrue(isOptimized(callLC));
assertThrows(() => callLC(undefined, undefined), TypeError);
assertTrue(isOptimized(callLC));

// Throwing toString on the receiver. The optimized path must not silently
// short-circuit via pointer-eq -- the throw has to propagate.
const o = { toString() { throw new Error("kaboom"); } };
assertThrows(() => callLC(o, o), Error, "kaboom");
assertTrue(isOptimized(callLC));

// --- Long strings: exercise the loop without unrolling into a single block.
const long_a = "x".repeat(64) + "0";
const long_b = "x".repeat(64) + "1";
check(long_a, long_b, -1);
check(long_b, long_a, +1);
check(long_a, long_a, 0);

// Tagged-equal long string (pointer-eq path after type guards).
const long_eq = "Aircraft ".repeat(8);
check(long_eq, long_eq, 0);
