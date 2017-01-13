// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

var Debug = debug.Debug;
var expected = ["debugger;", "debugger;"];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertEquals(expected.shift(), exec_state.frame(0).sourceLineText());
    exec_state.prepareStep(Debug.StepAction.StepNext);
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setListener(listener);
debugger;
