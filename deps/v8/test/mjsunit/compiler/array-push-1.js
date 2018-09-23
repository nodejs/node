// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Test multiple arguments push for PACKED_SMI_ELEMENTS.
(function() {
  function push0(a) {
    return a.push();
  }

  assertEquals(0, push0([]));
  assertEquals(1, push0([1]));
  %OptimizeFunctionOnNextCall(push0);
  assertEquals(2, push0([1, 2]));

  function push1(a) {
    return a.push(1);
  }

  assertEquals(1, push1([]));
  assertEquals(2, push1([1]));
  %OptimizeFunctionOnNextCall(push1);
  assertEquals(3, push1([1, 2]));

  function push2(a) {
    return a.push(1, 2);
  }

  assertEquals(2, push2([]));
  assertEquals(3, push2([1]));
  %OptimizeFunctionOnNextCall(push2);
  assertEquals(4, push2([1, 2]));

  function push3(a) {
    return a.push(1, 2, 3);
  }

  assertEquals(3, push3([]));
  assertEquals(4, push3([1]));
  %OptimizeFunctionOnNextCall(push3);
  assertEquals(5, push3([1, 2]));
})();

// Test multiple arguments push for HOLEY_SMI_ELEMENTS.
(function() {
  function push0(a) {
    return a.push();
  }

  assertEquals(1, push0(new Array(1)));
  assertEquals(2, push0(new Array(2)));
  %OptimizeFunctionOnNextCall(push0);
  assertEquals(3, push0(new Array(3)));

  function push1(a) {
    return a.push(1);
  }

  assertEquals(2, push1(new Array(1)));
  assertEquals(3, push1(new Array(2)));
  %OptimizeFunctionOnNextCall(push1);
  assertEquals(4, push1(new Array(3)));

  function push2(a) {
    return a.push(1, 2);
  }

  assertEquals(3, push2(new Array(1)));
  assertEquals(4, push2(new Array(2)));
  %OptimizeFunctionOnNextCall(push2);
  assertEquals(5, push2(new Array(3)));

  function push3(a) {
    return a.push(1, 2, 3);
  }

  assertEquals(4, push3(new Array(1)));
  assertEquals(5, push3(new Array(2)));
  %OptimizeFunctionOnNextCall(push3);
  assertEquals(6, push3(new Array(3)));
})();

// Test multiple arguments push for PACKED_DOUBLE_ELEMENTS.
(function() {
  function push0(a) {
    return a.push();
  }

  assertEquals(1, push0([1.1]));
  assertEquals(2, push0([1.1, 2.2]));
  %OptimizeFunctionOnNextCall(push0);
  assertEquals(3, push0([1.1, 2.2, 3.3]));

  function push1(a) {
    return a.push(1.1);
  }

  assertEquals(2, push1([1.1]));
  assertEquals(3, push1([1.1, 2.2]));
  %OptimizeFunctionOnNextCall(push1);
  assertEquals(4, push1([1.1, 2.2, 3.3]));

  function push2(a) {
    return a.push(1.1, 2.2);
  }

  assertEquals(3, push2([1.1]));
  assertEquals(4, push2([1.1, 2.2]));
  %OptimizeFunctionOnNextCall(push2);
  assertEquals(5, push2([1.1, 2.2, 3.3]));

  function push3(a) {
    return a.push(1.1, 2.2, 3.3);
  }

  assertEquals(4, push3([1.1]));
  assertEquals(5, push3([1.1, 2.2]));
  %OptimizeFunctionOnNextCall(push3);
  assertEquals(6, push3([1.1, 2.2, 3.3]));
})();

// Test multiple arguments push for HOLEY_DOUBLE_ELEMENTS.
(function() {
  function push0(a) {
    return a.push();
  }

  assertEquals(2, push0([, 1.1]));
  assertEquals(3, push0([, 1.1, 2.2]));
  %OptimizeFunctionOnNextCall(push0);
  assertEquals(4, push0([, 1.1, 2.2, 3.3]));

  function push1(a) {
    return a.push(1.1);
  }

  assertEquals(3, push1([, 1.1]));
  assertEquals(4, push1([, 1.1, 2.2]));
  %OptimizeFunctionOnNextCall(push1);
  assertEquals(5, push1([, 1.1, 2.2, 3.3]));

  function push2(a) {
    return a.push(1.1, 2.2);
  }

  assertEquals(4, push2([, 1.1]));
  assertEquals(5, push2([, 1.1, 2.2]));
  %OptimizeFunctionOnNextCall(push2);
  assertEquals(6, push2([, 1.1, 2.2, 3.3]));

  function push3(a) {
    return a.push(1.1, 2.2, 3.3);
  }

  assertEquals(5, push3([, 1.1]));
  assertEquals(6, push3([, 1.1, 2.2]));
  %OptimizeFunctionOnNextCall(push3);
  assertEquals(7, push3([, 1.1, 2.2, 3.3]));
})();

// Test multiple arguments push for PACKED_ELEMENTS.
(function() {
  function push0(a) {
    return a.push();
  }

  assertEquals(1, push0(['1']));
  assertEquals(2, push0(['1', '2']));
  %OptimizeFunctionOnNextCall(push0);
  assertEquals(3, push0(['1', '2', '3']));

  function push1(a) {
    return a.push('1');
  }

  assertEquals(2, push1(['1']));
  assertEquals(3, push1(['1', '2']));
  %OptimizeFunctionOnNextCall(push1);
  assertEquals(4, push1(['1', '2', '3']));

  function push2(a) {
    return a.push('1', '2');
  }

  assertEquals(3, push2(['1']));
  assertEquals(4, push2(['1', '2']));
  %OptimizeFunctionOnNextCall(push2);
  assertEquals(5, push2(['1', '2', '3']));

  function push3(a) {
    return a.push('1', '2', '3');
  }

  assertEquals(4, push3(['1']));
  assertEquals(5, push3(['1', '2']));
  %OptimizeFunctionOnNextCall(push3);
  assertEquals(6, push3(['1', '2', '3']));
})();

// Test multiple arguments push for HOLEY_ELEMENTS.
(function() {
  function push0(a) {
    return a.push();
  }

  assertEquals(2, push0([, '1']));
  assertEquals(3, push0([, '1', '2']));
  %OptimizeFunctionOnNextCall(push0);
  assertEquals(4, push0([, '1', '2', '3']));

  function push1(a) {
    return a.push('1');
  }

  assertEquals(3, push1([, '1']));
  assertEquals(4, push1([, '1', '2']));
  %OptimizeFunctionOnNextCall(push1);
  assertEquals(5, push1([, '1', '2', '3']));

  function push2(a) {
    return a.push('1', '2');
  }

  assertEquals(4, push2([, '1']));
  assertEquals(5, push2([, '1', '2']));
  %OptimizeFunctionOnNextCall(push2);
  assertEquals(6, push2([, '1', '2', '3']));

  function push3(a) {
    return a.push('1', '2', '3');
  }

  assertEquals(5, push3([, '1']));
  assertEquals(6, push3([, '1', '2']));
  %OptimizeFunctionOnNextCall(push3);
  assertEquals(7, push3([, '1', '2', '3']));
})();
