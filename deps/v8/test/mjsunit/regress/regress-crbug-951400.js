// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function foo(arr) {
  gc();
  eval(arr);
};
%PrepareFunctionForOptimization(foo);
try {
  foo("tag`Hello${tag}`");
} catch (e) {}

%OptimizeFunctionOnNextCall(foo);

try {
  foo("tag.prop`${tag}`");
} catch (e) {}
