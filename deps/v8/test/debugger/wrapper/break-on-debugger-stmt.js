// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() { debugger; debugger; }

const Debug = new DebugWrapper();
Debug.enable();

let breakEventCount = 0;
Debug.setListener(function(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  breakEventCount++;
});

assertEquals(0, breakEventCount);
f();
f();
f();
assertEquals(6, breakEventCount);
