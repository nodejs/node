// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Exercises every path of the conversions into Float64 and into HoleyFloat64:
// each source representation, each assumed input type, the cached alternatives,
// and the inputs that make the conversions deopt.

const packed = [1.5, 2.5, 3.5];        // PACKED_DOUBLE_ELEMENTS
const holey = [1.5, , 3.5];            // HOLEY_DOUBLE_ELEMENTS
const smis = [1, 2, 3];                // PACKED_SMI_ELEMENTS
const objects = [{}, {}];              // PACKED_ELEMENTS
const typed = new Float64Array([1.5, 2.5]);

// ---------------------------------------------------------------- to Float64

// The value is already a Float64.
(function Float64Source() {
  function f(a, i) { return Math.sqrt(a[i]); }
  %PrepareFunctionForOptimization(f);
  assertEquals(Math.sqrt(1.5), f(packed, 0));
  assertEquals(Math.sqrt(1.5), f(packed, 0));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(Math.sqrt(2.5), f(packed, 1));
  assertTrue(isMaglevved(f));
})();

// An Int32, and a Uint32.
(function IntSources() {
  function f(x) { return Math.sqrt(x | 0); }
  function g(x) { return Math.sqrt(x >>> 0); }
  %PrepareFunctionForOptimization(f);
  %PrepareFunctionForOptimization(g);
  assertEquals(2, f(4));
  assertEquals(2, f(4));
  assertEquals(2, g(4));
  // Warm g with a value above 2^31, so that it is really the Uint32 conversion
  // that gets compiled and the optimized call below does not deopt.
  assertEquals(Math.sqrt(2 ** 32 - 1), g(-1));
  %OptimizeMaglevOnNextCall(f);
  %OptimizeMaglevOnNextCall(g);
  assertEquals(3, f(9));
  assertEquals(3, g(9));
  assertEquals(Math.sqrt(2 ** 32 - 1), g(-1));
  assertTrue(isMaglevved(f));
  assertTrue(isMaglevved(g));
})();

// A tagged value known to be a Smi, which is untagged and converted from its
// Int32 representation.
(function TaggedSmiSource() {
  function f(a, i) { return Math.sqrt(a[i]); }
  %PrepareFunctionForOptimization(f);
  assertEquals(1, f(smis, 0));
  assertEquals(1, f(smis, 0));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(Math.sqrt(2), f(smis, 1));
  assertTrue(isMaglevved(f));
})();

// A tagged value that is a number but not a Smi.
(function TaggedNumberSource() {
  function f(o) { return Math.sqrt(o.x); }
  %PrepareFunctionForOptimization(f);
  assertEquals(1.5, f({x: 2.25}));
  assertEquals(1.5, f({x: 2.25}));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(2.5, f({x: 6.25}));
  // A non-number deopts rather than converting.
  assertEquals(NaN, f({x: "no"}));
})();

// A tagged oddball, converted the way ToNumber would.
(function TaggedOddballSource() {
  function f(v) { return v + 1; }
  %PrepareFunctionForOptimization(f);
  assertEquals(2.25, f(1.25));
  assertEquals(NaN, f(undefined));
  assertEquals(2, f(true));
  assertEquals(1, f(null));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(2.25, f(1.25));
  assertEquals(NaN, f(undefined));
  assertEquals(2, f(true));
  assertEquals(1, f(false));
  assertEquals(1, f(null));
})();

// A HoleyFloat64 whose use does not tolerate the hole: it deopts.
(function HoleySourceIntolerant() {
  function f(a, i) { return Math.sqrt(a[i]); }
  %PrepareFunctionForOptimization(f);
  assertEquals(Math.sqrt(1.5), f(holey, 0));
  assertEquals(Math.sqrt(1.5), f(holey, 0));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(Math.sqrt(3.5), f(holey, 2));
  assertEquals(NaN, f(holey, 1));  // Math.sqrt(undefined) is NaN
})();

// A HoleyFloat64 whose use tolerates the hole: it converts it to NaN.
(function HoleySourceTolerant() {
  function f(a, i) { return a[i] + 1; }
  %PrepareFunctionForOptimization(f);
  assertEquals(2.5, f(holey, 0));
  assertEquals(NaN, f(holey, 1));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(4.5, f(holey, 2));
  assertEquals(NaN, f(holey, 1));
  assertTrue(isMaglevved(f));
})();

// A HoleyFloat64 stored into a packed double array, where the hole is not a
// valid element and so has to deopt.
(function HoleySourceChecked() {
  function f(a, j, p, i) { p[i] = a[j]; }
  const warm = [1.5, 2.5, 3.5];
  %PrepareFunctionForOptimization(f);
  f(holey, 0, warm, 0);
  f(holey, 0, warm, 0);
  %OptimizeMaglevOnNextCall(f);

  const p = [1.5, 2.5, 3.5];
  f(holey, 2, p, 0);
  assertEquals(3.5, p[0]);
  f(holey, 1, p, 1);  // the hole is not a number: deopt, then store undefined
  assertEquals(undefined, p[1]);
})();

// A tagged value already known to be a number, whose Float64 was not kept
// across the merge, so it is converted again without a check.
(function KnownNumberSource() {
  function f(v, c) {
    let x = v * 1;
    if (c) { x = x + 1; }
    return Math.sqrt(v) + x;
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(3.75, f(2.25, false));
  assertEquals(4.75, f(2.25, true));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(8.75, f(6.25, false));
  assertEquals(9.75, f(6.25, true));
  assertTrue(isMaglevved(f));
})();

// An IntPtr, which is what a typed array length is.
(function IntPtrSource() {
  function f(t, h, i) { h[i] = t.length; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  f(typed, warm, 0);
  f(typed, warm, 0);
  %OptimizeMaglevOnNextCall(f);

  const h = [1.5, , 3.5, 4.5];
  f(typed, h, 0);
  assertEquals(2, h[0]);
  assertTrue(0 in h);
})();

// The type of the input contradicts what the conversion assumes.
(function ImpossibleType() {
  function f(a, i) { return a[i] * 2; }
  %PrepareFunctionForOptimization(f);
  assertEquals(NaN, f(objects, 0));
  assertEquals(NaN, f(objects, 0));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(NaN, f(objects, 1));
})();

// The Float64 of a value is reused once it has been converted.
(function ReusedFloat64Alternative() {
  function f(o) { return Math.sqrt(o.x) + Math.sqrt(o.x) + o.x; }
  %PrepareFunctionForOptimization(f);
  assertEquals(5.25, f({x: 2.25}));
  assertEquals(5.25, f({x: 2.25}));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(11.25, f({x: 6.25}));
  assertTrue(isMaglevved(f));
})();

// ----------------------------------------------------------- to HoleyFloat64

// Storing into a holey double array converts into HoleyFloat64. Each case
// checks that the element is a real element with the right value, i.e. that its
// bits were not left meaning the hole or undefined.
function store(v, i) {
  const h = [1.5, , 3.5, 4.5];
  h[i] = v;
  return h;
}

// The value is already a HoleyFloat64, hole included.
(function HoleyToHoley() {
  function f(a, j, h, i) { h[i] = a[j]; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  f(holey, 0, warm, 0);
  // Warm the hole too, so that the optimized call below does not deopt on it.
  f(holey, 1, warm, 0);
  %OptimizeMaglevOnNextCall(f);

  const h = [1.5, , 3.5, 4.5];
  f(holey, 2, h, 0);
  assertEquals(3.5, h[0]);
  f(holey, 1, h, 0);  // the hole reads as undefined and stays undefined
  assertTrue(0 in h);
  assertEquals(undefined, h[0]);
  assertTrue(isMaglevved(f));
})();

// A constant, including the undefined one.
(function ConstantToHoley() {
  function f(h, i) { h[i] = 1.5; }
  function g(h, i) { h[i] = undefined; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  %PrepareFunctionForOptimization(g);
  f(warm, 0); f(warm, 0); g(warm, 0); g(warm, 0);
  %OptimizeMaglevOnNextCall(f);
  %OptimizeMaglevOnNextCall(g);

  const h = [1.5, , 3.5, 4.5];
  f(h, 0);
  assertEquals(1.5, h[0]);
  g(h, 0);
  assertTrue(0 in h);
  assertEquals(undefined, h[0]);
})();

// An Int32 and a Uint32, which cannot mean the hole and so are widened for
// free.
(function IntToHoley() {
  function f(h, i, x) { h[i] = x | 0; }
  function g(h, i, x) { h[i] = x >>> 0; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  %PrepareFunctionForOptimization(g);
  f(warm, 0, 7); f(warm, 0, 7); g(warm, 0, 7); g(warm, 0, 7);
  %OptimizeMaglevOnNextCall(f);
  %OptimizeMaglevOnNextCall(g);

  const h = [1.5, , 3.5, 4.5];
  f(h, 0, 9);
  assertEquals(9, h[0]);
  g(h, 0, -1);
  assertEquals(2 ** 32 - 1, h[0]);
  assertTrue(0 in h);
})();

// A tagged Smi, which goes through the Int32 conversion.
(function TaggedSmiToHoley() {
  function f(a, j, h, i) { h[i] = a[j]; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  f(smis, 0, warm, 0);
  f(smis, 0, warm, 0);
  %OptimizeMaglevOnNextCall(f);

  const h = [1.5, , 3.5, 4.5];
  f(smis, 1, h, 0);
  assertEquals(2, h[0]);
  assertTrue(0 in h);
})();

// A tagged number that is not a Smi.
(function TaggedNumberToHoley() {
  function f(o, h, i) { h[i] = o.x; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  f({x: 2.25}, warm, 0);
  f({x: 2.25}, warm, 0);
  %OptimizeMaglevOnNextCall(f);

  const h = [1.5, , 3.5, 4.5];
  f({x: 6.25}, h, 0);
  assertEquals(6.25, h[0]);
  assertTrue(0 in h);
})();

// A tagged oddball: undefined is a valid element of a holey double array, the
// others deopt or convert as ToNumber would.
(function TaggedOddballToHoley() {
  function f(v, h, i) { h[i] = v; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  f(2.25, warm, 0);
  f(undefined, warm, 0);
  %OptimizeMaglevOnNextCall(f);

  const h = [1.5, , 3.5, 4.5];
  f(6.25, h, 0);
  assertEquals(6.25, h[0]);
  f(undefined, h, 0);
  assertTrue(0 in h);
  assertEquals(undefined, h[0]);
})();

// A Float64 that cannot be one of the NaN patterns is widened for free, and one
// that could be is silenced first.
(function Float64ToHoley() {
  function arith(a, j, h, i) { h[i] = a[j] * 2; }
  function load(t, j, h, i) { h[i] = t[j]; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(arith);
  %PrepareFunctionForOptimization(load);
  arith(packed, 0, warm, 0); arith(packed, 0, warm, 0);
  load(typed, 0, warm, 0); load(typed, 0, warm, 0);
  %OptimizeMaglevOnNextCall(arith);
  %OptimizeMaglevOnNextCall(load);

  const h = [1.5, , 3.5, 4.5];
  arith(packed, 1, h, 0);
  assertEquals(5, h[0]);
  assertTrue(0 in h);
  load(typed, 1, h, 0);
  assertEquals(2.5, h[0]);
  assertTrue(0 in h);
})();

// A value whose type contradicts what the store assumes.
(function ImpossibleTypeToHoley() {
  function f(a, j, h, i) { h[i] = a[j]; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  f(objects, 0, warm, 0);
  f(objects, 0, warm, 0);
  %OptimizeMaglevOnNextCall(f);

  const h = [1.5, , 3.5, 4.5];
  f(objects, 1, h, 0);
  assertEquals(objects[1], h[0]);
})();

// The HoleyFloat64 of a value is reused once it has been converted.
(function ReusedHoleyAlternative() {
  function f(o, h, i, j) { h[i] = o.x; h[j] = o.x; }
  const warm = [1.5, , 3.5, 4.5];
  %PrepareFunctionForOptimization(f);
  f({x: 2.25}, warm, 0, 2);
  f({x: 2.25}, warm, 0, 2);
  %OptimizeMaglevOnNextCall(f);

  const h = [1.5, , 3.5, 4.5];
  f({x: 6.25}, h, 0, 2);
  assertEquals(6.25, h[0]);
  assertEquals(6.25, h[2]);
  assertTrue(isMaglevved(f));
})();
