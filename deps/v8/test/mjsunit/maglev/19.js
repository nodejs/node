// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f(i) {
  var j;
  var o;
  if (i) {
  } else {
    if (j) {
    } else {
    }
  }
  return o;
}

%PrepareFunctionForOptimization(f);
f(false, true);

%OptimizeMaglevOnNextCall(f);
f(false, true);
f(false, true);
f(false, true);
