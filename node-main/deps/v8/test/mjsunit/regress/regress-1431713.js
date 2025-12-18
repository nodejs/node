// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
  const unused = -x;
}

%PrepareFunctionForOptimization(foo);
foo(2316465375n);
%OptimizeFunctionOnNextCall(foo);
assertThrows(() => foo({__proto__: Object(Symbol)}), TypeError);
