// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

let listenerComplete = false;
let breakPointCount = 0;
let exceptionThrown = false;

async function f() {
  await (async function() { var a = "a"; await 1; debugger; })();

  var b = "b";

  assertTrue(listenerDone);
  assertFalse(exceptionThrown);
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
    exceptionThrown = true;
  };
};

Debug.setListener(listener);

var c = "c";
f();
