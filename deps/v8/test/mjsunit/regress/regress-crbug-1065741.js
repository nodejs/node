// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function bar() {
  String.prototype.startsWith.apply();
}

%PrepareFunctionForOptimization(bar);
assertThrows(bar, TypeError);
assertThrows(bar, TypeError);
%OptimizeFunctionOnNextCall(bar);
assertThrows(bar, TypeError);
%PrepareFunctionForOptimization(bar);
%OptimizeFunctionOnNextCall(bar);
assertThrows(bar, TypeError);
assertOptimized(bar);
