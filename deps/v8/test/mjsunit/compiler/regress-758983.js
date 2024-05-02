// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var holey = [ 2.2,,"x" ];

function f(b) {
  holey[0] = 1.1;
  var r = holey[0];
  r = b ? r : 0;
  return r < 0;
}

%PrepareFunctionForOptimization(f);
f(true);
f(true);
%OptimizeFunctionOnNextCall(f);
assertFalse(f(true));
