// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Debug = debug.Debug;

let break_count = 0;
let exception = null;

const obj = { x: 42 };
function m() {
  return () => (a = this) => {
    debugger;
    return a;
  };
}

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertEquals(obj, exec_state.frame(0).evaluate("this").value());
    assertTrue(exec_state.frame(0).evaluate("this === obj").value());
    break_count++;
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

Debug.setListener(listener);
assertEquals(obj, m.call(obj)()());
Debug.setListener(null);

assertEquals(1, break_count);
assertNull(exception);
