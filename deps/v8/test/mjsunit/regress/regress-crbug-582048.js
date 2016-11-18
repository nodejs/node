// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

var Debug = debug.Debug;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var frame_count = exec_state.frameCount();
    for (var i = 0; i < frame_count; i++) {
      var frame = exec_state.frame(i);
      var scope_count = frame.scopeCount();
      for (var j = 0; j < scope_count; j++) {
        var scope = frame.scope(j);
        assertTrue(scope.scopeObject().property('').isUndefined());
      }
    }
  } catch (e) {
    print(e, e.stack);
    exception = e;
  }
}

Debug.setListener(listener);

(function(a = 1) { debugger; })();

Debug.setListener(null);
