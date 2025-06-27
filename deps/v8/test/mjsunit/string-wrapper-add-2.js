// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

var stringWrapper = new String('constant');

function add(a) {
  return a + stringWrapper;
}
%PrepareFunctionForOptimization(add);

const string = 'first';

assertEquals('firstconstant', add(string));
%OptimizeFunctionOnNextCall(add);

assertEquals('firstconstant', add(string));
assertOptimized(add);

// Invalidate the protector.
stringWrapper.valueOf = () => 'value';

assertUnoptimized(add);
assertEquals('firstvalue', add(string));
