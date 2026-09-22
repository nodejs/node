// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-analyze-environment-liveness --no-experimental-reuse-locals-blocklists

// Test that debug-evaluate only resolves variables that are used by
// the function inside which we debug-evaluate. This is to avoid
// incorrect variable resolution when a context-allocated variable is
// shadowed by a stack-allocated variable.

Debug = debug.Debug

let test_name;
let listener_delegate;
let listener_called;
let exception;
let begin_test_count = 0;
let end_test_count = 0;
let break_count = 0;

// Debug event listener which delegates.
function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      break_count++;
      listener_called = true;
      listener_delegate(exec_state);
    }
  } catch (e) {
    exception = e;
    print(e, e.stack);
  }
}
Debug.setListener(listener);

function BeginTest(name) {
  test_name = name;
  listener_called = false;
  exception = null;
  begin_test_count++;
}

function EndTest() {
  assertTrue(listener_called, "listener not called for " + test_name);
  assertNull(exception, test_name + " / " + exception);
  end_test_count++;
}

BeginTest("Check that 'x' resolves correctly and 'a' is written correctly");
var a = "a";
function f1() {
  var x = 1;     // context allocate x
  (() => x);
  var y = "y";
  var z = "z";
  (function () {
    var x = 2;   // stack allocate shadowing x
    (function () {
      y;         // access y
      debugger;  // ReferenceError
    })();        // 2
  })();          // 1
  return y;
}

listener_delegate = function(exec_state) {
  for (var i = 0; i < exec_state.frameCount() - 1; i++) {
    var frame = exec_state.frame(i);
    var value;
    try {
      value = frame.evaluate("x").value();
    } catch (e) {
      value = e.name;
    }
    print(frame.sourceLineText());
    var expected = frame.sourceLineText().match(/\/\/ (.*$)/)[1];
    assertEquals(String(expected), String(value));
  }
  assertEquals("[object global]",
                String(exec_state.frame(0).evaluate("this").value()));
  assertEquals("y", exec_state.frame(0).evaluate("y").value());
  assertEquals("a", exec_state.frame(0).evaluate("a").value());
  exec_state.frame(0).evaluate("a = 'A'");
  assertThrows(() => exec_state.frame(0).evaluate("z"), ReferenceError);
}
f1();
assertEquals("A", a);
a = "a";
EndTest();

BeginTest("Check that a context-allocated 'this' works")
function f2() {
  var x = 1;     // context allocate x
  (() => x);
  var y = "y";
  var z = "z";
  (function() {
    var x = 2;   // stack allocate shadowing x
    (() => {
      y;
      a;
      this;      // context allocate receiver
      debugger;  // ReferenceError
    })();        // 2
  })();          // 1
  return y;
};

// Uses the same listener delgate as for `f1`.
f2();
assertEquals("A", a);
EndTest();

BeginTest("Check that we don't get confused with nested scopes");
function f3() {
  var x = 1;     // context allocate x
  (() => x);
  (function() {
    var x = 2;   // stack allocate shadowing x
    (function() {
      {          // context allocate x in a nested scope
        let x = 3;
        (() => x);
      }
      debugger;
    })();
  })();
}

listener_delegate = function(exec_state) {
  assertThrows(() => exec_state.frame(0).evaluate("x").value());
}
f3();
EndTest();

BeginTest("Check that stack-allocated variable is unavailable");
function f4() {
  let a = 1;
  let b = 2;
  let c = 3;
  () => a + c;  // a and c are context-allocated
  return function g() {
    let a = 2;  // a is stack-allocated
    return function h() {
      b;  // b is allocated onto f's context.
      debugger;
    }
  }
}

listener_delegate = function(exec_state) {
  assertEquals(2, exec_state.frame(0).evaluate("b").value());
  assertEquals(3, exec_state.frame(0).evaluate("c").value())
  assertThrows(() => exec_state.frame(0).evaluate("a").value());
};
(f4())()();
EndTest();

BeginTest("Check that block lists on the closure boundary work as expected");
function f5() {
  let a = 1;
  () => a;  // a is context-allocated
  return function g() {
    let a = 2;  // a is stack-allocated
    {
      let b = 3;
      return function h() {
        debugger;
      }
    }
  }
}

listener_delegate = function(exec_state) {
  assertThrows(() => exec_state.frame(0).evaluate("a").value());
};
(f5())()();
EndTest();

BeginTest("Check evaluating on outer closure scopes with shadowing");
function test_scopes_shadowing() {
  let x = 'outer_x';
  let y = 'outer_y';
  () => [x, y];  // context allocate x and y in f7
  return function g() {
    let x = 'middle_x';
    () => x;  // context allocate x in g
    return function h() {
      let x = 'inner_x_stack';  // stack allocate x in h
      debugger;
    };
  };
}

listener_delegate = function(exec_state) {
  assertEquals('inner_x_stack', exec_state.frame(0).scope(0).evaluate("x").value());
  assertEquals('outer_y', exec_state.frame(0).scope(0).evaluate("y").value());

  assertEquals('middle_x', exec_state.frame(0).scope(1).evaluate("x").value());
  assertEquals('outer_y', exec_state.frame(0).scope(1).evaluate("y").value());

  assertEquals('outer_x', exec_state.frame(0).scope(2).evaluate("x").value());
  assertEquals('outer_y', exec_state.frame(0).scope(2).evaluate("y").value());
};
(test_scopes_shadowing())()();
EndTest();

BeginTest("Check that outer functions also get the correct block list calculated");
// This test is important once we reuse block list info. The block list for `g`
// needs to be correctly calculated already when we stop on break_position 1.

let break_position;
function f6() {
  let a = 1;                             // stack-allocated
  return function g() {                  // g itself doesn't require a context.
    if (break_position === 2) debugger;
    let a = 2; (() => a);                // context-allocated
    return function h() {
      if (break_position === 1) debugger;
    }
  }
}

listener_delegate = function (exec_state) {
  assertEquals(2, exec_state.frame(0).evaluate("a").value());
}
break_position = 1;
(f6())()();
EndTest();

BeginTest("Check that outer functions also get the correct block list calculated (continued)");
listener_delegate = function (exec_state) {
  assertThrows(() => exec_state.frame(0).evaluate("a").value());
}
break_position = 2;
(f6())()();
EndTest();

BeginTest("Check evaluating on outer closure scope with stack-allocated shadowed variable");
var x = 'global_x';
function test_outer_shadow() {
  let x = 'outer_stack';
  let closure_y = 'closure_y';
  () => closure_y; // force context allocation for closure_y
  return function inner() {
    debugger;
  };
}
listener_delegate = function(exec_state) {
  assertThrows(() => exec_state.frame(0).scope(1).evaluate("x").value(), ReferenceError);
  assertEquals('closure_y', exec_state.frame(0).scope(1).evaluate("closure_y").value());
};
(test_outer_shadow())();
EndTest();

assertEquals(begin_test_count, break_count,
  'one or more tests did not enter the debugger');
assertEquals(begin_test_count, end_test_count,
  'one or more tests did not have its result checked');
