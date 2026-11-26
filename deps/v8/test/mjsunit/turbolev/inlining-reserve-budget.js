// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function scaleColor(c, s) {
  return {
    r: c.r * s,
    g: c.g * s,
    b: c.b * s,
  };
}

function step(c) {
  // 0.5 is a HeapNumber constant; scaleColor should be treated as a small
  // function with heap number inputs even when the regular budget is tight.
  const scaled = scaleColor(c, 0.5);
  return scaled.r + scaled.g + scaled.b;
}

const c = { r: 1.5, g: 2.5, b: 3.5 };
%PrepareFunctionForOptimization(scaleColor);
%PrepareFunctionForOptimization(step);
assertEquals(3.75, step(c));
assertEquals(3.75, step(c));
%OptimizeFunctionOnNextCall(step);
assertEquals(3.75, step(c));
assertOptimized(step);

// Non-eagerly-inlined callee returning Float64 (overwritten with ReturnedValue)
// whose result feeds a sibling candidate.
function computeScale(n) {
  let acc = 0.125;
  if (n > 0) acc += 0.125;
  if (n > 1) acc += 0.25;
  return acc;
}

function stepFromReturnedFloat(c, n) {
  const s = computeScale(n);
  const scaled = scaleColor(c, s);
  return scaled.r + scaled.g + scaled.b;
}

%PrepareFunctionForOptimization(computeScale);
%PrepareFunctionForOptimization(stepFromReturnedFloat);
assertEquals(3.75, stepFromReturnedFloat(c, 2));
assertEquals(3.75, stepFromReturnedFloat(c, 2));
%OptimizeFunctionOnNextCall(stepFromReturnedFloat);
assertEquals(3.75, stepFromReturnedFloat(c, 2));
assertOptimized(stepFromReturnedFloat);
