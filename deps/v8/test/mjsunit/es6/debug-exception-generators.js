// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

Debug = debug.Debug

var exception = null;
var log = [];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    var match = /Exception (\w)/.exec(line);
    assertNotNull(match);
    assertEquals(match[1], event_data.exception());
    log.push(match[1]);
  } catch (e) {
    exception = e;
  }
}


function* g() {
  try {
    throw "a";  // Ordinary throw. Exception a
  } catch (e) {}
  try {
    yield 1;  // Caught internally. Exception b
  } catch (e) {}
  yield 2;
  yield 3;    // Caught externally. Exception c
  yield 4;
}

Debug.setListener(listener);
Debug.setBreakOnException();
var g_obj = g();
assertEquals(1, g_obj.next().value);
assertEquals(2, g_obj.throw("b").value);
assertEquals(3, g_obj.next().value);
assertThrows(() => g_obj.throw("c"));
assertThrows(() => g_obj.throw("d"));  // Closed generator. Exception d
Debug.setListener(null);
Debug.clearBreakOnException();
assertEquals(["a", "b", "c", "d"], log);
assertNull(exception);
