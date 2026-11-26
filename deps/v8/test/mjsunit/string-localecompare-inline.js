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

// --- 4-byte chunk boundaries across all byte positions.
// Build strings via Array.join("") so lengths >= 13 stay flat SeqOneByteStrings
// instead of ConsStrings/SlicedStrings.
for (const len of [3, 4, 5, 6, 8, 9, 16, 32]) {
  const chars = Array.from("callsign".repeat(4).slice(0, len));
  const base = chars.join("");
  check(base, chars.join(""), 0);
  for (let pos = 0; pos < len; pos++) {
    const pre = chars.slice(0, pos).join("");
    const suf = chars.slice(pos + 1).join("");
    check([pre, "a", suf].join(""), [pre, "b", suf].join(""), -1);
    check([pre, "a", suf].join(""), [pre, "A", suf].join(""), -1);
    check([pre, "\xff", suf].join(""), [pre, "a", suf].join(""), +1);
    if (pos + 1 < len) {
      const suf2 = chars.slice(pos + 2).join("");
      check([pre, "az", suf2].join(""), [pre, "ba", suf2].join(""), -1);
      check([pre, "ba", suf2].join(""), [pre, "az", suf2].join(""), +1);
    }
  }
}

// Tagged-equal long string (pointer-eq path after type guards).
const long_eq = "Aircraft ".repeat(8);
check(long_eq, long_eq, 0);

// --- Constant operands. A string literal is baked into the code as a
// constant, so the compiler knows its shape and guards accordingly. Feed the
// other shapes in as arguments: each must reach the builtin instead of being
// walked as one-byte characters.
function constCmp(x) { return "Aircraft 0".localeCompare(x); }
%PrepareFunctionForOptimization(constCmp);
constCmp("Aircraft 1");
constCmp("Aircraft 1");
%OptimizeFunctionOnNextCall(constCmp);
// Several of these have equal content and differ only in shape, so the shape
// is what the failure message has to name.
for (const [shape, arg] of [["seq one-byte", "Aircraft 1"],
                            ["equal content", "Aircraft 0"],
                            ["two-byte", "Aircraft 中"],
                            ["cons", "Air" + "craft 1"],
                            ["sliced", "xAircraft 1".substring(1)]]) {
  assertEquals(Math.sign(unopt("Aircraft 0", arg)), Math.sign(constCmp(arg)),
               `constCmp(${shape})`);
}

// Two-byte constant receiver, which the inline can never handle. The
// character must be outside Latin-1 ("café" is still stored one-byte) and
// must come first, since reading a two-byte body as one-byte only changes
// the answer once the walk reaches it.
const kTwoByte = "中af";
function constTwoByte(x) { return "中af".localeCompare(x); }
%PrepareFunctionForOptimization(constTwoByte);
constTwoByte("zzz");
constTwoByte("zzz");
%OptimizeFunctionOnNextCall(constTwoByte);
for (let c = 0; c < 128; c++) {
  const arg = String.fromCharCode(c) + "af";
  assertEquals(Math.sign(unopt(kTwoByte, arg)), Math.sign(constTwoByte(arg)),
               `constTwoByte(${JSON.stringify(arg)})`);
}
assertEquals(0, constTwoByte(kTwoByte));

// Short constant operands (lengths 0..4, as receiver and as argument) cover
// eliding the 4-byte prefix loop when either operand has constant length < 4.
const shortConstants = [
  ["", (x) => "".localeCompare(x), (x) => x.localeCompare("")],
  ["f", (x) => "f".localeCompare(x), (x) => x.localeCompare("f")],
  ["fo", (x) => "fo".localeCompare(x), (x) => x.localeCompare("fo")],
  ["f\xff", (x) => "f\xff".localeCompare(x), (x) => x.localeCompare("f\xff")],
  ["fox", (x) => "fox".localeCompare(x), (x) => x.localeCompare("fox")],
  ["foxy", (x) => "foxy".localeCompare(x), (x) => x.localeCompare("foxy")],
];
const shortInputs = [
  "", "e", "f", "g", "F", "\xff",
  "fa", "fo", "fp", "Fo", "f\xff",
  "foa", "fox", "foy", "foX", "fo\xff",
  "foxa", "foxy", "foxz", "foxY", "fox\xff", "foxy0",
  ["f", "o"].join(""), ["f", "\xff"].join(""), ["f", "o", "x"].join(""),
  ["fo", "xy"].join(""),
  %ConstructConsString("fox", "yyyyyyyyyy"),
  %ConstructSlicedString("xfoxyzzzzzzzzzz", 1),
  "中",
];
for (const [cst, cmpLeft, cmpRight] of shortConstants) {
  %PrepareFunctionForOptimization(cmpLeft);
  %PrepareFunctionForOptimization(cmpRight);
  cmpLeft("abc");
  cmpRight("abc");
  %OptimizeFunctionOnNextCall(cmpLeft);
  %OptimizeFunctionOnNextCall(cmpRight);
  for (const arg of shortInputs) {
    assertEquals(Math.sign(unopt(cst, arg)), Math.sign(cmpLeft(arg)),
                 `cmpLeft(${JSON.stringify(cst)}, ${JSON.stringify(arg)})`);
    assertEquals(Math.sign(unopt(arg, cst)), Math.sign(cmpRight(arg)),
                 `cmpRight(${JSON.stringify(arg)}, ${JSON.stringify(cst)})`);
  }
  assertEquals(Math.sign(unopt(cst, 5)), Math.sign(cmpLeft(5)));
}

// Same-operand calls fold TaggedEqual so the prefix walk is unreachable during
// MachineLowering; also cover constant and tagged Smi arguments.
function cmpSelf(x) { return x.localeCompare(x); }
function cmpConstSelf() { return "fox".localeCompare("fox"); }
function cmpConstSmi() { return "5".localeCompare(5); }
function cmpTaggedSmi(x, n) { return x.localeCompare(n | 0); }
for (const fn of [cmpSelf, cmpConstSelf, cmpConstSmi, cmpTaggedSmi]) {
  %PrepareFunctionForOptimization(fn);
  fn("5", 5);
  fn("5", 5);
  %OptimizeFunctionOnNextCall(fn);
}
assertEquals(0, cmpSelf("fox"));
assertEquals(0, cmpSelf("中"));
assertEquals(0, cmpSelf(%ConstructConsString("fox", "yyyyyyyyyy")));
assertEquals(0, cmpConstSelf());
assertEquals(0, cmpConstSmi());
assertEquals(0, cmpTaggedSmi("5", 5));
assertEquals(-1, Math.sign(cmpTaggedSmi("4", 5)));
