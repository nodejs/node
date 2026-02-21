// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const v = [];
function foo() {
  Int8Array.prototype.values.call([v]);
}

%PrepareFunctionForOptimization(foo);
assertThrows(foo, TypeError);
assertThrows(foo, TypeError);
%OptimizeFunctionOnNextCall(foo);
assertThrows(foo, TypeError);
