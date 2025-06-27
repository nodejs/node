// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --noanalyze-environment-liveness


var Debug = debug.Debug;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertEquals(2, exec_state.frameCount());
    assertEquals("a", exec_state.frame(0).localName(0));
    assertEquals(1, exec_state.frame(0).localValue(0).value());
    assertEquals(1, exec_state.frame(0).localCount());
  } catch (e) {
    exception = e;
  }
}

function f() {
  var a = 1;
  {
    let b = 2;
    debugger;
  }
}

Debug.setListener(listener);
f();
Debug.setListener(null);
assertNull(exception);
