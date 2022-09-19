// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function OSRInsideTry(x) {
  try {
    for (var i = 0; i < 10; i++) { if (i == 5) %OptimizeOsr(); }
    throw x;
  } catch (e) {
    return e + 1;
  }
  return x + 2;
}
%PrepareFunctionForOptimization(OSRInsideTry);
assertEquals(24, OSRInsideTry(23));


function OSRInsideCatch(x) {
  try {
    throw x;
  } catch (e) {
    for (var i = 0; i < 10; i++) { if (i == 5) %OptimizeOsr(); }
    return e + 1;
  }
  return x + 2;
}
%PrepareFunctionForOptimization(OSRInsideCatch);
assertEquals(24, OSRInsideCatch(23));


function OSRInsideFinally_Return(x) {
  try {
    throw x;
  } finally {
    for (var i = 0; i < 10; i++) { if (i == 5) %OptimizeOsr(); }
    return x + 1;
  }
  return x + 2;
}
%PrepareFunctionForOptimization(OSRInsideFinally_Return);
assertEquals(24, OSRInsideFinally_Return(23));


function OSRInsideFinally_ReThrow(x) {
  try {
    throw x;
  } finally {
    for (var i = 0; i < 10; i++) { if (i == 5) %OptimizeOsr(); }
  }
  return x + 2;
}
%PrepareFunctionForOptimization(OSRInsideFinally_ReThrow);
assertThrows("OSRInsideFinally_ReThrow(new Error)", Error);
