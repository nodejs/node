// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Debug = debug.Debug;

var frame;

Debug.setListener(function (event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    frame = exec_state.frame(0);

    // Try changing the value, which hasn't yet been initialized.
    assertEquals(3, frame.evaluate("result = 3").value());
    assertEquals(3, frame.evaluate("result").value());
  }
});

function makeCounter() {
  // The debug-evaluate should be able to set this to 3. This was fixed with
  // https://crbug.com/1246907 which delays debug breaks triggered by stack
  // checks until the function context is properly set up.
  assertEquals(3, result);

  var result = 0;

  // Regardless of how `result` is allocated, it should now be initialized.
  assertEquals(0, result);

  // Close over `result` to cause it to be heap-allocated.
  return () => ++result;
}

// Break on entry to a function which includes heap-allocated variables.
%ScheduleBreak();
makeCounter();

// Check the frame state which was collected during the breakpoint.
assertEquals(1, frame.localCount());
assertEquals('result', frame.localName(0));
assertEquals(undefined, frame.localValue(0).value());
assertEquals(3, frame.scopeCount());
assertEquals(debug.ScopeType.Local, frame.scope(0).scopeType());
assertEquals(debug.ScopeType.Script, frame.scope(1).scopeType());
assertEquals(debug.ScopeType.Global, frame.scope(2).scopeType());
