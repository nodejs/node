// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that debug-evaluate properly shadows stack-allocated variables.

Debug = debug.Debug

let exception = null;
function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertThrows(() => exec_state.frame(0).evaluate("a").value());
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

Debug.setListener(listener);

(function f() {
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
})()()();

Debug.setListener(null);
assertNull(exception);
