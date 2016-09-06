// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

var Debug = debug.Debug;
var steps = 0;
var exception = null;

function listener(event, execState, eventData, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertEquals([ debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global],
                 execState.frame().allScopes().map(s => s.scopeType()));
    var x_value = execState.frame().evaluate("x").value();
    if (steps < 2) {
      assertEquals(undefined, x_value);
      execState.prepareStep(Debug.StepAction.StepIn);
    } else {
      assertEquals("l => l", x_value.toString());
    }
    steps++;
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

(function() {
  debugger;
  var x = l => l;
})();

Debug.setListener(null);
assertNull(exception);
assertEquals(3, steps);
