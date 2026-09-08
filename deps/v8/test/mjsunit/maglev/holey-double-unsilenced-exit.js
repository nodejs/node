// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// A HoleyFloat64 -> Float64 conversion of a hole-tolerant use does not silence
// the hole and undefined NaN patterns anymore, so the resulting Float64 can
// carry them. Everything that gives those bits back their meaning (storing them
// into a double array, or rematerializing them on deopt) has to canonicalize
// them itself, otherwise a plain NaN would read back as a hole or as undefined.

// (1) ToNumber of a hole, stored into a packed double array that later becomes
// holey: the element must survive the elements-kind transition.
(function() {
  function f(a, i, out) { out[0] = +a[i]; }
  const a = [1.5, , 2.5];
  const warmup = [7.5, 8.5];
  %PrepareFunctionForOptimization(f);
  f(a, 0, warmup);
  f(a, 0, warmup);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5];  // PACKED_DOUBLE_ELEMENTS
  f(a, 1, out);            // out[0] = +undefined = NaN
  assertTrue(isNaN(out[0]));
  out[4] = 9.5;            // -> HOLEY_DOUBLE_ELEMENTS
  assertTrue(0 in out);
  assertTrue(isNaN(out[0]));
})();

// (2) Same, for an arithmetic use whose operation folds away.
(function() {
  function f(a, i, out) { out[0] = a[i] * 1; }
  const a = [1.5, , 2.5];
  const warmup = [7.5, 8.5];
  %PrepareFunctionForOptimization(f);
  f(a, 0, warmup);
  f(a, 0, warmup);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5];
  f(a, 1, out);
  assertTrue(isNaN(out[0]));
  out[4] = 9.5;
  assertTrue(0 in out);
  assertTrue(isNaN(out[0]));
})();

// (3) The same value used to initialize a double array literal.
(function() {
  function f(a, i) { return [+a[i], 1.5]; }
  const a = [1.5, , 2.5];
  %PrepareFunctionForOptimization(f);
  f(a, 0);
  f(a, 0);
  %OptimizeMaglevOnNextCall(f);

  const out = f(a, 1);
  assertTrue(isNaN(out[0]));
  out[4] = 9.5;
  assertTrue(0 in out);
  assertTrue(isNaN(out[0]));
})();

// (4) Stored back into a holey double array, it must stay a NaN element rather
// than becoming a hole or an undefined.
(function() {
  function f(a, i, out) { out[1] = +a[i]; }
  const a = [1.5, , 2.5];
  const warmup = [7.5, 8.5, , 9.5];
  %PrepareFunctionForOptimization(f);
  f(a, 0, warmup);
  f(a, 0, warmup);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5, , 9.5];  // HOLEY_DOUBLE_ELEMENTS
  f(a, 1, out);
  assertTrue(1 in out);
  assertTrue(isNaN(out[1]));
})();

// (5) Rematerialized on a lazy deopt, it must be a NaN and not an undefined.
(function() {
  function f(a, i) {
    const x = +a[i];
    %DeoptimizeFunction(f);
    return x;
  }
  const a = [1.5, , 2.5];
  %PrepareFunctionForOptimization(f);
  assertEquals(1.5, f(a, 0));
  assertEquals(1.5, f(a, 0));
  %OptimizeMaglevOnNextCall(f);
  assertTrue(isNaN(f(a, 1)));
})();

// (6) A HoleyFloat64 phi feeding an untagging that tolerated undefined must not
// deopt when it sees a hole.
(function() {
  function f(a, i, j, c) {
    const x = c ? a[i] : a[j];
    return x + 1;
  }
  const a = [1.5, , 2.5, 3.5];
  %PrepareFunctionForOptimization(f);
  assertEquals(2.5, f(a, 0, 2, true));
  assertEquals(3.5, f(a, 0, 2, false));
  assertTrue(isNaN(f(a, 1, 2, true)));  // undefined + 1 = NaN
  %OptimizeMaglevOnNextCall(f);
  assertEquals(2.5, f(a, 0, 2, true));
  assertTrue(isMaglevved(f));
  assertTrue(isNaN(f(a, 1, 2, true)));
  assertTrue(isMaglevved(f));
})();
