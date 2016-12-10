// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:  --harmony-async-await --allow-natives-syntax --expose-debug-as debug

// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

listenerComplete = false;
breakPointCount = 0;

async function f() {
  await (async function() { var a = "a"; await 1; debugger; })();

  var b = "b";

  assertTrue(listenerDone);
  assertFalse(exception);
  assertEquals(1, breakpointCount);
}

function listener(event, exec_state, event_data, data) {
  try {
    if (event != Debug.DebugEvent.Break) return;

    breakpointCount++;
    listenerDone = true;
    assertEquals("a", exec_state.frame(0).evaluate("a"));
    assertEquals("b", exec_state.frame(1).evaluate("b"));
    assertEquals("c", exec_state.frame(2).evaluate("c"));
  } catch (e) {
    exception = e;
  };
};

Debug.setListener(listener);

var c = "c";
f();
