// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Debug = debug.Debug;
var listened = false;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var foo_arguments = exec_state.frame(1).evaluate("arguments").value();
    var bar_arguments = exec_state.frame(0).evaluate("arguments").value();
    assertArrayEquals(foo_expected, foo_arguments);
    assertArrayEquals(bar_expected, bar_arguments);
    listened = true;
  } catch (e) {
    print(e);
    print(e.stack);
  }
}

Debug.setListener(listener);

function foo(a) {
  function bar(a,b,c) {
    debugger;
    return a + b + c;
  }
  return bar(1,2,a);
}
%PrepareFunctionForOptimization(foo);

listened = false;
foo_expected = [3];
bar_expected = [1,2,3];
assertEquals(6, foo(3));
assertTrue(listened);

listened = false;
foo_expected = [3];
bar_expected = [1,2,3];
assertEquals(6, foo(3));
assertTrue(listened);

listened = false;
foo_expected = [3];
bar_expected = [1,2,3];
%OptimizeFunctionOnNextCall(foo);
assertEquals(6, foo(3));
assertTrue(listened);

listened = false;
foo_expected = [3,4,5];
bar_expected = [1,2,3];
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
assertEquals(6, foo(3,4,5));
assertTrue(listened);

Debug.setListener(null);
