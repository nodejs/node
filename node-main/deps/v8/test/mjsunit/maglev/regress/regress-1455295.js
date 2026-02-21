// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo() {
  let caught = 0;
  try {
    [].forEach(undefined);
  } catch (e) {
    caught += 2;
  }
  try {
    [].forEach(undefined);
  } catch (e) {
    caught += 40;
  }
  return caught;
}

%PrepareFunctionForOptimization(foo);
print(42, foo());
print(42, foo());
%OptimizeMaglevOnNextCall(foo);
print(42, foo());
