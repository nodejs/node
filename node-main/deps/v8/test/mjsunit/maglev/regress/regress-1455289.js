// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(i) {
  i++;
  try {
    [].forEach(i);
  } catch (e) {
    return 42;
  }
}

%PrepareFunctionForOptimization(foo);
print(42, foo(0));
print(42, foo(0));
%OptimizeMaglevOnNextCall(foo);
print(42, foo(0));
