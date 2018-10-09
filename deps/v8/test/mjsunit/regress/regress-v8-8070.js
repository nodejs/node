// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(iterator) {
  for (const entry of iterator) {}
}

%NeverOptimizeFunction(bar);

function foo(a) {
  const iterator = a.values();
  bar(iterator);
  return iterator.next().done;
}

const a = [1, 2, 3];
assertTrue(foo(a));
assertTrue(foo(a));
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo(a));
