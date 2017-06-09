// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Inlining shift with holey smi arrays shouldn't deopt just because it
// encounters the hole on the copy step.
function doShift(a) {
  var x = a.shift();
  return x;
}

function makeArray() {
  var a = [1, 2,, 3];
  a[0] = 2;
  return a;
}

doShift(makeArray());
doShift(makeArray());
%OptimizeFunctionOnNextCall(doShift);
doShift(makeArray());
assertOptimized(doShift);
