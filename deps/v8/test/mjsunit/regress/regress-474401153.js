// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function bar(a) {
  return a[0];
}

function foo(...args) {
  return bar(args, holey_doubles);
}

const holey_doubles = [,3.14,5.1];

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);

bar(holey_doubles)
bar(["ab", 5]    )
bar(holey_doubles)
bar(["ab", 5]    )

assertEquals(1, foo(1, 3.14));

%OptimizeFunctionOnNextCall(foo);
assertEquals(1, foo(1, 3.14));
