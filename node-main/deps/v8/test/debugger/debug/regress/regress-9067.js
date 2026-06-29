// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code --flush-bytecode

var Debug = debug.Debug
var bp;

Debug.setListener(function (event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    Debug.clearBreakPoint(bp);
    gc();
  }
});

function f() {
  (function () {})();
}

bp = Debug.setBreakPoint(f, 0, 0);
f();
