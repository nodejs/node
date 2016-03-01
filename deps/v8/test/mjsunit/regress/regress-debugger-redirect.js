// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --legacy-const

function f(x) {
  // This function compiles into code that only throws a redeclaration
  // error. It contains no stack check and has no function body.
  const x = 0;
  return x;
}

function g() {
  f(0);
}

var exception = null;
var called = false;
var Debug = debug.Debug;
Debug.setBreakOnException();

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    called = true;
    Debug.setBreakPoint(f, 1);
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

assertThrows(g);
assertNull(exception);
assertTrue(called);
