// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = {0: 0, 1: 1, '01': 7};
function foo(index) {
    return [v[index], v[index + 1], index + 1];
};

%PrepareFunctionForOptimization(foo);
assertEquals(foo(0), [0, 1, 1]);
assertEquals(foo(0), [0, 1, 1]);
%OptimizeFunctionOnNextCall(foo);
assertEquals(foo(0), [0, 1, 1]);
assertEquals(foo('0'), [0, 7, '01']);
