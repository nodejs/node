// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-generators --expose-debug-as debug

var Debug = debug.Debug;

var listener_called;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    listener_called = true;
    exec_state.frame().allScopes();
  }
}

Debug.setListener(listener);

function *generator_local_2(a) {
  debugger;
}
generator_local_2(1).next();

assertTrue(listener_called, "listener not called");
