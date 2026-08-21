// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function broken(value) {
  return Math.floor(value / 65536);
}
function toUnsigned(i) {
  return i >>> 0;
}
function outer(i) {
  return broken(toUnsigned(i));
};
%PrepareFunctionForOptimization(outer);
for (var i = 0; i < 5; i++) outer(0);
broken(0x80000000);  // Spice things up with a sprinkling of type feedback.
%OptimizeFunctionOnNextCall(outer);
assertEquals(32768, outer(0x80000000));
