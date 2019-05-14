// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Collect the actual properties looked up on the Proxy.
const actual = [];

// Perform a relational comparison with a Proxy on the right hand
// side and a Symbol which cannot be turned into a Number on the
// left hand side.
function foo() {
  actual.length = 0;
  const lhs = Symbol();
  const rhs = new Proxy({}, {
    get: function(target, property, receiver) {
      actual.push(property);
      return undefined;
    }
  });
  return lhs < rhs;
}

assertThrows(foo, TypeError);
assertEquals([Symbol.toPrimitive, 'valueOf', 'toString'], actual);
assertThrows(foo, TypeError);
assertEquals([Symbol.toPrimitive, 'valueOf', 'toString'], actual);
%OptimizeFunctionOnNextCall(foo);
assertThrows(foo, TypeError);
assertEquals([Symbol.toPrimitive, 'valueOf', 'toString'], actual);
