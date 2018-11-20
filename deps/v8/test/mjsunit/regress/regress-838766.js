// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo(x) {
  x = x | 2147483648;
  return Number.parseInt(x + 65535, 8);
}
assertEquals(-72161, foo());
assertEquals(-72161, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(-72161, foo());
