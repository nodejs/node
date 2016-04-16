// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition-filter=f --expose-debug-as debug

var Debug = debug.Debug;

var break_count = 0;

function f() {
  debugger;
}

function listener(event, exec_data) {
  if (event != Debug.DebugEvent.Break) return;
  break_count++;
}

f();
assertEquals(0, break_count);

Debug.setListener(listener);

f();
assertEquals(1, break_count);

Debug.setListener(null);

f();
assertEquals(1, break_count);
