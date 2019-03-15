// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const re = /abc/;

// Move the test method one prototype up.
re.__proto__.__proto__.test = re.__proto__.test;
delete re.__proto__.test;

function foo(s) {
  return re.test(s);
}

assertTrue(foo('abc'));
assertTrue(foo('abc'));
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo('abc'));
assertFalse(foo('ab'));
