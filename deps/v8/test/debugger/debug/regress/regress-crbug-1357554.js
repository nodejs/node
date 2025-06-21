// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-sparkplug

var Debug = debug.Debug;

Debug.setListener(function (event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    Debug.setListener(null);
    Debug.stepInto();
  }
});

%ScheduleBreak();
(function foo() {
  const x = 5;
  () => x; // context-allocate x.
})();
